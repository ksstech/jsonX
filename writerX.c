/*
 * writerX.c
 * Copyright 2014-20 AM Maree/KSS Technologies (Pty) Ltd.
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
 *
 */

#include	"writerX.h"
#include	"x_string_general.h"
#include	"printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include	"syslog.h"
#include	"hal_config.h"

#include	<string.h>

#define	debugFLAG					0xC000

#define	debugBUILD					(debugFLAG & 0xC000)
#define	debugSTATE					(debugFLAG & 0x0001)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

static	int32_t	ecJsonDecimals = xpfDEFAULT_DECIMALS ;
static const char ESChars[] = { CHR_BACKSLASH,CHR_DOUBLE_QUOTE,CHR_FWDSLASH,CHR_BS,CHR_FF,CHR_TAB,CHR_LF,CHR_CR,CHR_NUL } ;

int32_t	xJsonMapNumType(varsize_t vs, varform_t vf) {
	if (vs > vs64B || vf > vfFXX) {
		IF_myASSERT(debugPARAM, 0) ;
		return jsonFORM_NAN ;
	}
	int32_t idx = (vs << 2) + vf ;
	IF_TRACK(debugTRACK, "vs=%d vf=%d idx=%d", vs, vf, idx) ;
	IF_myASSERT(debugRESULT, idx != jsonFORM_NAN) ;
	return idx ;
}

/**
 * ecJsonAddChar() - write a single char to the stream
 * @param pJson
 * @param cChar
 * @return
 */
static int32_t	ecJsonAddChar(json_obj_t * pJson, char cChar) {
	if (xUBufSpace(pJson->psBuf) == 0)
		return erFAILURE ;
	uprintfx(pJson->psBuf, "%c", cChar) ;
	return erSUCCESS ;
}

/**
 * ecJsonAddChars() - write a null terminated string with escapes to the stream
 * @param pJson
 * @param pString
 * @return
 */
static int32_t	ecJsonAddChars(json_obj_t * pJson, const char * pString) {
	int32_t	xLen = xstrlen(pString) ;					// Step 1: determine the string length
	IF_myASSERT(debugSTATE, xLen > 0) ;
	while (xLen--) {									// Step 2: handle characters (with optional escapes)
		if (strchr(ESChars, *pString) != NULL) {			// Step 3: handle ESCape if required
		   ecJsonAddChar(pJson, CHR_BACKSLASH) ;
		}
		ecJsonAddChar(pJson, *pString++) ;				// Step 4: process the actual character
	}
	return erSUCCESS ;
}

/**
 * ecJsonAddString() Add a string, including opening/closing quotes, to the stream
 * @param pJson
 * @param pString
 * @return
 */
static int32_t  ecJsonAddString(json_obj_t * pJson, const char * pString) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(pString)) ;		// can be in FLASH or SRAM
	ecJsonAddChar(pJson, CHR_DOUBLE_QUOTE) ;			// Step 1: write the opening ' " '
	ecJsonAddChars(pJson, pString) ;					// Step 2: write the string
	return ecJsonAddChar(pJson, CHR_DOUBLE_QUOTE) ;		// Step 3: write the closing ' " '
}

/**
 * ecJsonAddStringArray() - add and array of strings to the stream
 * @param pJson
 * @param pValue
 * @param xSize
 * @return
 */
static	int32_t  ecJsonAddStringArray(json_obj_t * pJson, p32_t pValue, size_t xSize) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(pValue.pvoid));	// can be in FLASH or SRAM
	ecJsonAddChar(pJson, CHR_L_SQUARE) ;				// Step 1: write the opening ' [ '
	while (xSize--) {									// Step 2: handle each string from array, 1 by 1
		ecJsonAddString(pJson, *pValue.ppc8++) ;		// Step 2a: add the string
		if (xSize != 0) {								// Step 2b: if not last value ?
			ecJsonAddChar(pJson, CHR_COMMA) ; 			// 			add a separator
		}
	}
	return ecJsonAddChar(pJson, CHR_R_SQUARE) ;			// Step 3: write the closing ' ] '
}

/**
 * ecJsonAddNumber() - write a value, using the correct format, to the stream
 * @param pJson
 * @param pValue
 * @param NumType
 * @return
 */
static	int32_t  ecJsonAddNumber(json_obj_t * pJson, p32_t pValue, uint8_t Form) {
	x64_t		xVal ;
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(pValue.pvoid) ) ;
	switch(Form) {									// Normalize the size to 64bit double
	case cvU08:	xVal.u64	= *pValue.pu8 ;		break ;
	case cvU16:	xVal.u64	= *pValue.pu16 ;	break ;
	case cvU32:	xVal.u64	= *pValue.pu32 ;	break ;
	case cvU64:	xVal.u64	= *pValue.pu64 ;	break ;
	case cvI08:	xVal.i64	= *pValue.pi8 ;		break ;
	case cvI16:	xVal.i64	= *pValue.pi16 ;	break ;
	case cvI32:	xVal.i64	= *pValue.pi32 ;	break ;
	case cvI64:	xVal.i64	= *pValue.pi64 ;	break ;
	case cvF32:	xVal.f64	= *pValue.pf32 ;	break ;
	case cvF64:	xVal.f64	= *pValue.pf64 ;	break ;
	default:	IF_myASSERT(debugSTATE, 0) ;	return erJSON_FORMAT ;
	}
// Step 2: write the value, format depending on fractional part
	if (Form==jsonFORM_I08 || Form==jsonFORM_I16 || Form==jsonFORM_I32 || Form==jsonFORM_I64)
		uprintfx(pJson->psBuf, "%lld", xVal.i64) ;
	else if (Form==jsonFORM_U08 || Form==jsonFORM_U16 || Form==jsonFORM_U32 || Form==jsonFORM_U64)
		uprintfx(pJson->psBuf, "%llu", xVal.u64) ;
	else if (Form==jsonFORM_X08 || Form==jsonFORM_X16 || Form==jsonFORM_X32 || Form==jsonFORM_X64)
		uprintfx(pJson->psBuf, "0x%llx", xVal.u64) ;
	else
		uprintfx(pJson->psBuf, "%.*f", ecJsonDecimals, xVal.f64) ;

	if (xUBufSpace(pJson->psBuf) == 0)
		return erJSON_BUF_FULL ;
	return  erSUCCESS ;
}

/**
 * ecJsonAddTimeStamp()
 * @param pJson		- JSON structure on which to operate
 * @param pValue	- pointer to the TSZ structure to use
 * @param eFormType	- format in which to write the timestamp
 * @return
 */
static int32_t  ecJsonAddTimeStamp(json_obj_t * pJson, p32_t pValue, uint8_t eForm) {
	switch(eForm) {
	case jsonFORM_DT_UTC:	uprintfx(pJson->psBuf, "\"%R\"", *pValue.pu64) ;	break ;
	case jsonFORM_DT_TZ:	uprintfx(pJson->psBuf, "\"%+Z\"", pValue.pvoid) ;	break ;
	case jsonFORM_DT_ELAP:	uprintfx(pJson->psBuf, "\"%!R\"", *pValue.pu64) ;	break ;
	case jsonFORM_DT_ALT:	uprintfx(pJson->psBuf, "\"%#Z\"", pValue.pvoid) ;	break ;
	default:
		IF_myASSERT(debugPARAM, 0) ;
		return erJSON_FORMAT ;
	}
	if (xUBufSpace(pJson->psBuf) == 0)
		return erJSON_BUF_FULL ;
	return  erSUCCESS ;
}

/**
 * ecJsonAddNumberArray() - add and array of number, any format and size, to the stream
 * Assume the members of the array are all there and ready to be added.
 * Will open, fill and close the array, with a leading COMMA is other values
 * already in the object..
 */
static	int32_t  ecJsonAddNumberArray(json_obj_t * pJson, p32_t pValue, uint8_t eForm, size_t xSize) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(pValue.pvoid)) ;// can be in FLASH or SRAM
	ecJsonAddChar(pJson, CHR_L_SQUARE) ;				// Step 1: write the opening ' [ '

	while (xSize--) {									// Step 2: handle each array value, 1 by 1
		ecJsonAddNumber(pJson, pValue, eForm) ;			// Step 2a: add the number
		if (xSize != 0)									// Step 2b: if not the last number
			ecJsonAddChar(pJson, CHR_COMMA) ;  			//			write separating ','
		if (eForm <= jsonFORM_X08)						// Step 3: adjust the value pointer
			pValue.pu8++ ;
		else if (eForm <= jsonFORM_X16)
			pValue.pu16++ ;
		else if (eForm <= jsonFORM_X32)
			pValue.pu32++ ;
		else if (eForm <= jsonFORM_X64)
			pValue.pu64++ ;
		else
			return erJSON_NUM_TYPE ;
	}
	return ecJsonAddChar(pJson, CHR_R_SQUARE) ;			// Step 4: write the closing ' ] '
}

/**
 * ecJsonSetDecimals()	Set the number of decimals to display
 * @brief Set the number of default float digits, if invalid parameter reset to default
 * @param xNumber		Number of digits to set
 * @return				erSUCCESS if xNumber in range, else erFAILURE
 */
int32_t	ecJsonSetDecimals(int32_t xNumber) {
	if (INRANGE(0, xNumber, xpfMAXIMUM_DECIMALS, int32_t)) {
		ecJsonDecimals = xNumber ;
		return erSUCCESS ;
	}
	ecJsonDecimals = xpfDEFAULT_DECIMALS ;
	return erFAILURE ;
}

/**
 * ecJsonAddKeyValue() - add a key : value[number array] pair
 * \brief
 * \param[in]	JSON object to build into
 * \param[in]	string to use as key value
 * \param[in]	variable type based on Complex Var definitions
 * \param[in]	type of value being jsonXXXX (NULL / FALSE / TRUE / NUMBER / STRING / ARRAY / OBJECT)
 * \param[in]	number type being jsonFORM_xxx (Ixx / Uxx / Fxx | x08 / x16 / x32 / x64 || NAN)
 * \param[in]	number of items in array (or 1 if not an array)
 * \return
 * \note:	In the case of adding a new object, pValue must be a pointer to the location
 * 			of the the new Json object struct to be filled in....
 */
int32_t  ecJsonAddKeyValue(json_obj_t * pJson, const char * pKey, p32_t pValue, uint8_t eType, uint8_t eForm, size_t xArrSize) {
	json_obj_t * pJson1	;
	IF_SL_INFO(debugTRACK, "p1=%p  p2=%s  p3=%p  p4=%d  p5=%d  p6=%d", pJson, pKey, pValue, eType, eForm, xArrSize) ;
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pJson) && halCONFIG_inSRAM(pJson->psBuf) && halCONFIG_inMEM(pValue.pvoid)) ;

	if (pJson->val_count > 0)							// Step 1: if already something in the object
		ecJsonAddChar(pJson, CHR_COMMA) ;				//			write separating ','

	if (pKey != 0) {									// Step 2: If key supplied
		ecJsonAddString(pJson, pKey) ;					//			add it...
		ecJsonAddChar(pJson, CHR_COLON) ;
	}
	switch(eType) {										// Step 3: Add the value
	case jsonNULL: ecJsonAddChars(pJson, "null") ;		break ;
	case jsonFALSE:	ecJsonAddChars(pJson, "false") ;	break ;
	case jsonTRUE: ecJsonAddChars(pJson, "true") ;		break ;
	case jsonNUMBER:
		IF_myASSERT(debugSTATE, xArrSize == 1) ;
		ecJsonAddNumber(pJson, pValue, eForm) ;
		break ;

	case jsonSTRING:
		IF_myASSERT(debugSTATE, xArrSize == 1) ;
		ecJsonAddString(pJson, pValue.pc8) ;
		break ;

	case jsonDATETIME: ecJsonAddTimeStamp(pJson, pValue, eForm) ;	break ;

	case jsonARRAY:
		IF_myASSERT(debugSTATE, xArrSize > 0) ;
		if (eForm <= jsonFORM_X64) {
			ecJsonAddNumberArray(pJson, pValue, eForm, xArrSize) ;
		} else if (eForm == jsonFORM_STRING) {
			ecJsonAddStringArray(pJson, pValue, xArrSize) ;
		} else {
			IF_myASSERT(debugRESULT, 0) ;
			return erJSON_ARRAY ;
		}
		break ;

	case jsonOBJECT:
		IF_myASSERT(debugPARAM, halCONFIG_inMEM(pValue.pvoid) ) ;	// can be in FLASH or SRAM
		pJson1	= (json_obj_t *) pValue.pvoid ;
		ecJsonCreateObject(pJson1, pJson->psBuf) ; 		// create new object with same buffer
		pJson->child	= pJson1 ;						// setup link from parent to child
		pJson1->parent	= pJson ;						// setup link from child to parent
		pJson->obj_nest++ ;								// increase parent nest level
		break ;

	default:
		IF_myASSERT(debugRESULT, 0) ;
		return erJSON_TYPE ;
	}
	if (eType != jsonOBJECT) {							// for all key:value pairs other than OBJECT
		pJson->val_count++ ;							// increase the object count
	}
	IF_SL_INFO(debugBUILD, "%.*s", pJson->psBuf->Used, pJson->psBuf->pBuf) ;
	return erSUCCESS ;
}

/**
 * ecJsonCloseObject() - Close Json structure and write the closing '}' to the stream
 * @param pJson
 * @return
 */
int32_t  ecJsonCloseObject(json_obj_t * pJson) {
	if (pJson->child) {									// Is this a "parent" with a "child" ?
		ecJsonCloseObject(pJson->child) ;				// recurse to close the child first..
	}
	IF_myASSERT(debugPARAM, pJson->obj_nest == 0) ;		// should be zero after recursing to lowest level
	ecJsonAddChar(pJson, CHR_R_CURLY) ;					// close the current object
	if (pJson->parent) {								// is this a child to a parent ?
		pJson->parent->obj_nest-- ;						// adjust the nesting level of the parent
		pJson->parent->child	= 0 ;					// reset parent to child link
		pJson->parent			= 0 ;					// reset child to parent link
	}
	return  erSUCCESS ;
}

/**
 * ecJsonCreateObject() - Initialise new Json structure and write the opening '{' to the stream
 * @param pJson
 * @param psBuf
 * @return
 */
int32_t  ecJsonCreateObject(json_obj_t * pJson, ubuf_t * psBuf) {
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pJson) && halCONFIG_inSRAM(psBuf)) ;
	pJson->parent		= pJson->child	= 0 ;
	pJson->psBuf		= psBuf ;
	pJson->val_count	= 0 ;
	pJson->obj_nest		= 0 ;
	ecJsonAddChar(pJson, CHR_L_CURLY) ;
	return  erSUCCESS ;
}

#if 0

/**
 * ecJsonStartArray() - Start a new Json array and write the opening '[' to the stream
 * @param pJson
 * @param pKey
 * @return
 */
int32_t  ecJsonStartArray(json_obj_t * pJson, const char * pKey) {
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pJson)) ;
	if (pJson->val_count > 0) {							// Step 1: if already something in the object
		ecJsonAddChar(pJson, CHR_COMMA) ;				// yes, write separating ','
		pJson->val_count = 0 ;
	}
	if (pKey != 0) {									// Step 2: If key supplied, add it...
		ecJsonAddString(pJson, pKey) ;
		ecJsonAddChar(pJson, CHR_COLON) ;
	}
	ecJsonAddChar(pJson, CHR_L_SQUARE) ;				// Step 3: Start the open array
	pJson->arr_nest++ ;
	return  erSUCCESS ;
}

/**
 * ecJsonCloseArray() - Close a Json array and write the closing ']' to the stream
 * @param pJson
 * @return
 */
int32_t  ecJsonCloseArray(json_obj_t * pJson) {
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pJson)) ;
	ecJsonAddChar(pJson, CHR_R_SQUARE) ;
	pJson->arr_nest-- ;
	pJson->val_count = 1 ;								// flags something in object/array, separator required..
	return  erSUCCESS ;
}
#endif
