/*
 * writerX.c - Copyright (c) 2014-25 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 *
 * Add a complete item :
 * 	to an object
 * 	to an array
 *
 * Start a new OPEN:
 * 	object		Always start and leave open, cannot allow writing a complete object in 1 step...
 * 	array		Do we need now, can we stick to force adding an array in 1 go?
 *
 * Logical flow sequence:
 *	MUST start with ecJsonCreateObject() to create root/[grand]parent object
 *	Then using ecJsonAddKeyValue() can add { key:value [, key:value ..] } pairs
 *		Any key:value of type ARRAY added MUST be a complete item,
 *			and the array members MUST be of same type, STRING or NUMBER, only
 *		If the key:value type is OBJECT ecJsonAddKeyValue() will create a new OPEN child object
 *
 * Current restrictions:
 * 	No ARRAY within another ARRAY (ie nesting) catered for.
 *	Currently support string only and numbers only arrays, not mixed or other
 *	Ad hoc opening & closing of arrays not yet supported
 *	\uxxxx (four hex digits) not yet escaped...
 */

#include "hal_platform.h"
#include "hal_memory.h"
#include "hal_options.h"
#include "report.h"				// +x_definitions +stdarg +stdint +stdio
#include "syslog.h"
#include "writerX.h"
#include "string_general.h"

#include <string.h>

#define	debugFLAG					0xF000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

static int	ecJsonDecimals = xpfDEFAULT_DECIMALS;
static const char ESChars[] = { '\\', '"', '/', '\b', '\f', '\t', '\n', '\r', '\0' };

/**
 * @brief		write a single char to the stream
 * @param[in]	pJson - pointer to control structure
 * @param[in]	cChar - character to be added
 * @return		number of characters added or erFAILURE
 */
static void ecJsonAddChar(json_obj_t * pJson, char cChar) {
	uprintfx(pJson->psUB, "%c", cChar);
	IF_myASSERT(debugRESULT, xUBufGetSpace(pJson->psUB) > 0);
}

/**
 * @brief		write corrected escaped string to the stream
 * @param[in]	pJson - pointer to control structure
 * @param[in]	pStr - characters to be added
 * @return		number of characters added or erFAILURE
 */
static void ecJsonAddChars(json_obj_t * pJson, const char * pStr, size_t Sz) {
	if (Sz == 0) Sz = strlen(pStr);						// Step 1: determine the string length
	while (Sz--) {										// Step 2: handle characters (with optional escapes)
		if (strchr(ESChars, *pStr)) ecJsonAddChar(pJson, CHR_BACKSLASH);
		ecJsonAddChar(pJson, *pStr++);
	}
}

/**
 * ecJsonAddString() Add a string, including opening/closing quotes, to the stream
 * @param pJson
 * @param pString
 * @return
 */
static void ecJsonAddString(json_obj_t * pJson, const char * pStr, size_t Sz) {
	ecJsonAddChar(pJson, CHR_DOUBLE_QUOTE);				// Step 1: write the opening ' " '
	ecJsonAddChars(pJson, pStr, Sz);					// Step 2: write the string
	ecJsonAddChar(pJson, CHR_DOUBLE_QUOTE);				// Step 3: write the closing ' " '
}

/**
 * @brief	add and array of strings to the stream
 * @param	pJson
 * @param	pValue
 * @param	xSize
 * @return
 */
static void ecJsonAddArrayStrings(json_obj_t * pJson, px_t pX, size_t Sz) {
	ecJsonAddChar(pJson, CHR_L_SQUARE);					// Step 1: write the opening ' [ '
	while (Sz--) {										// Step 2: handle each string from array, 1 by 1
		ecJsonAddString(pJson, *pX.ppc8++, 0);			// Step 2a: add the string
		if (Sz != 0) ecJsonAddChar(pJson, CHR_COMMA);
	}
	ecJsonAddChar(pJson, CHR_R_SQUARE);					// Step 3: write the closing ' ] '
}

/**
 * @brief			write a value, using the correct format, to the stream
 * @param pJson
 * @param pValue
 * @param NumType
 * @return
 */
static void ecJsonAddNumber(json_obj_t * pJson, px_t pX, cvi_e cvI) {
	x64_t X64;
	switch(cvI) {										// Normalize size to X64
	case cvU08:	X64.u64	= *pX.pu8; break;
	case cvU16:	X64.u64	= *pX.pu16; break;
	case cvU32:	X64.u64	= *pX.pu32; break;
	case cvU64:	X64.u64	= *pX.pu64; break;
	case cvI08:	X64.i64	= *pX.pi8; break;
	case cvI16:	X64.i64	= *pX.pi16; break;
	case cvI32:	X64.i64	= *pX.pi32; break;
	case cvI64:	X64.i64	= *pX.pi64; break;
	case cvF32:	X64.f64	= *pX.pf32; break;
	case cvF64:	X64.f64	= *pX.pf64; break;
	default: IF_myASSERT(debugTRACK, 0); return;
	}
	// Step 2: write the value, format depending on fractional part
	uprintfx(pJson->psUB, cvI < cvI08 ? "%llu" : cvI < cvF32 ? "%lld" : "%g" , X64.f64);
	IF_myASSERT(debugRESULT, xUBufGetSpace(pJson->psUB) > 0);
}

/**
 * @brief	add and array of number, any format and size, to the stream
 * @brief	Assume the members of the array are all there and ready to be added.
 * @brief	Will open, fill and close the array, with a leading COMMA is other values
 * @brief	already in the object..
 */
static void ecJsonAddArrayNumbers(json_obj_t * pJson, px_t pX, cvi_e cvI, size_t Sz) {
	ecJsonAddChar(pJson, CHR_L_SQUARE);				// Step 1: write the opening ' [ '
	while (Sz--) {									// Step 2: handle each array value, 1 by 1
		ecJsonAddNumber(pJson, pX, cvI);			// Step 2a: add the number & optional comma separator
		if (Sz != 0) ecJsonAddChar(pJson, CHR_COMMA);
		pX.pv += xIndex2Bytes(cvI);					// Step 3: adjust the source value address
	}
	ecJsonAddChar(pJson, CHR_R_SQUARE);				// Step 4: write the closing ' ] '
}

static json_obj_t * ecJsonAddObject(json_obj_t * pJson, px_t pX) {
	json_obj_t * pJson1	= (json_obj_t *) pX.pv;
	ecJsonCreateObject(pJson1, pJson->psUB); 			// create new object with same buffer
	pJson->child = pJson1;								// setup link from parent to child
	pJson1->parent = pJson;								// setup link from child to parent
	pJson->obj_nest++;									// increase parent nest level
	return pJson1;
}

static json_obj_t * ecJsonAddArrayObject(json_obj_t * pJson, px_t pX) {
	IF_myASSERT(debugPARAM, halMemorySRAM(pX.pv));	// MUST be in SRAM
	ecJsonAddChar(pJson, CHR_L_SQUARE);					// Step 1: write the opening '['
	return ecJsonAddObject(pJson, pX);					// Step 2: create the object '{'
}

#if	(jsonHAS_TIMESTAMP == 1)
/**
 * @brief
 * @param pJson		- JSON structure on which to operate
 * @param pValue	- pointer to the TSZ structure to use
 * @param eFormType	- format in which to write the timestamp
 * @return
 */
static void ecJsonAddTimeStamp(json_obj_t * pJson, px_t pValue, cvi_e cvI) {
	switch(cvI) {
	case cvDT_ELAP: uprintfx(pJson->psUB, "\"%!R\"", *pValue.pu64);	break;
	case cvDT_UTC: uprintfx(pJson->psUB, "\"%R\"", *pValue.pu64);	break;
	case cvDT_ALT: uprintfx(pJson->psUB, "\"%#Z\"", pValue.pv);	break;
	case cvDT_TZ: uprintfx(pJson->psUB, "\"%+Z\"", pValue.pv);	break;
	default: IF_myASSERT(debugPARAM, 0); 			return;
	}
	IF_myASSERT(debugRESULT, xUBufGetSpace(pJson->psUB) > 0);
}
#endif

/**
 * ecJsonSetDecimals()	Set the number of decimals to display
 * @brief Set the number of default float digits, if invalid parameter reset to default
 * @param xNumber		Number of digits to set
 * @return				erSUCCESS if xNumber in range, else erFAILURE
 */
void ecJsonSetDecimals(int xNumber) { ecJsonDecimals = INRANGE(0, xNumber, xpfMAXIMUM_DECIMALS) ? xNumber : xpfDEFAULT_DECIMALS; }

/**
 * ecJsonAddKeyValue() - add a key : value[number array] pair
 * @brief
 * @param[in]	JSON object to build into
 * @param[in]	string to use as key value
 * @param[in]	pointer to variable(s) to be encoded
 * @param[in]	type of value being jsonXXXX (NULL / FALSE / TRUE / NUMBER / STRING / ARRAY / OBJECT)
 * @param[in]	number type being cvxxx (Ixx / Uxx / Fxx | x08 / x16 / x32 / x64 || NAN)
 * @param[in]	number of items in array (or 1 if not an array)
 * @return
 * @note:	In the case of adding a new object, pValue must be a pointer to the location
 * 			of the the new Json object struct to be filled in....
 */
int	ecJsonAddKeyValue(json_obj_t * pJson, const char * pKey, px_t pX, jform_t jForm, cvi_e cvI, size_t Sz) {
	u8_t Option = xOptionGet(dbgJSONwr);
	IF_PX(debugTRACK && Option, "p1=%p  p2=%s  p3=%p  p4=%hhu  p5=%hhu  p6=%zu", (void *)pJson, pKey, pX.pv, jForm, cvI, Sz);
	IF_myASSERT(debugPARAM, halMemorySRAM(pJson) && halMemorySRAM(pJson->psUB) && halMemoryANY(pX.pv));

	if (pJson->val_count > 0) ecJsonAddChar(pJson, CHR_COMMA);
	if (pKey != 0) {									// Step 2: If key supplied
		ecJsonAddString(pJson, pKey, 0);				//			add it...
		ecJsonAddChar(pJson, CHR_COLON);
	}
	switch(jForm) {										// Step 3: Add the value
	case jsonNULL: ecJsonAddChars(pJson, "null", sizeof("null") - 1); break;
	case jsonFALSE: ecJsonAddChars(pJson, "false", sizeof("false") - 1); break;
	case jsonTRUE: ecJsonAddChars(pJson, "true", sizeof("true") - 1); break;
	case jsonXXX: ecJsonAddNumber(pJson, pX, cvI); break;			// Sz ignored
	case jsonSXX: ecJsonAddString(pJson, pX.pc8, Sz); break;
	#if	(jsonHAS_TIMESTAMP == 1)
	case jsonEDTZ: ecJsonAddTimeStamp(pJson, pX, cvI); break;		// Sz ignored
	#endif
	case jsonARRAY:
		if (cvI == cvXXX) ecJsonAddArrayObject(pJson, pX)->type = jsonTYPE_ARRAY;	// Sz ignored
		else {
			IF_myASSERT(debugPARAM, Sz > 0);
			if (cvI <= cvSXX) ecJsonAddArrayNumbers(pJson, pX, cvI, Sz);
			else if (cvI == cvSXX) ecJsonAddArrayStrings(pJson, pX, Sz);
			else return erJSON_ARRAY;
		}
		break;
	case jsonOBJ: ecJsonAddObject(pJson, pX); break;
	default: IF_myASSERT(debugRESULT, 0); return erJSON_TYPE;
	}
	if (jForm != jsonOBJ) pJson->val_count++;
	IF_PX(debugTRACK && Option, "%.*s", pJson->psUB->Used, pJson->psUB->pBuf);
	return erSUCCESS;
}

/**
 * @brief	Close Json structure and write the closing '}' to the stream
 * @param	pJson
 * @return
 */
int	ecJsonCloseObject(json_obj_t * pJson) {
	if (pJson->child) ecJsonCloseObject(pJson->child);	// recurse to close the child first..
	IF_myASSERT(debugPARAM, pJson->obj_nest == 0);		// should be zero after recursing to lowest level
	ecJsonAddChar(pJson, CHR_R_CURLY);					// close the object
	if (pJson->type == jsonTYPE_ARRAY) ecJsonAddChar(pJson, CHR_R_SQUARE);				// close the array
	if (pJson->parent) {								// is this a child to a parent ?
		pJson->parent->obj_nest--;						// adjust the nesting level of the parent
		pJson->parent->child = 0;						// reset parent to child link
		pJson->parent = 0;								// reset child to parent link
	}
	return  erSUCCESS;
}

/**
 * @brief	Initialise new Json structure and write the opening '{' to the stream
 * @param	pJson
 * @param	psUB
 * @return
 */
int	ecJsonCreateObject(json_obj_t * pJson, ubuf_t * psUB) {
	IF_myASSERT(debugPARAM, halMemorySRAM(pJson) && halMemorySRAM(psUB));
	pJson->parent = pJson->child = 0;
	pJson->psUB = psUB;
	pJson->val_count = 0;
	pJson->obj_nest = 0;
	pJson->type = jsonTYPE_NULL;
	ecJsonAddChar(pJson, CHR_L_CURLY);
	return erSUCCESS;
}
