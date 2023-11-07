/*
 * parserX.c
 * Copyright 2014-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 *
 */

#include "hal_config.h"
#include "hal_options.h"
#include "parserX.h"
#include "printfx.h"									// x_definitions stdarg stdint stdio
#include "syslog.h"
#include "x_errors_events.h"
#include "x_string_general.h"
#include "x_string_to_values.h"

// ############################### BUILD: debug configuration options ##############################

#define	debugFLAG					0xF000

#define	debugPARSE					(debugFLAG & 0x0001)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

const char * tokType[] = { "Undef", "Object", "Array", "", "String", "", "", "", "Value" };

// ####################################### Global Functions ########################################

void xJsonTrackToken(char * pcBuf, jsmntok_t * psT) {
	if (psT->type != JSMN_UNDEFINED || psT->start || psT->end || psT->size) {
		CP("t=%d/%s  i=%d  s=%d  e=%d  l=%d '%.*s'\r\n", psT->type, tokType[psT->type], psT->size,
			psT->start, psT->end, psT->end - psT->start, psT->end - psT->start, pcBuf + psT->start);
	} else {
		CP("[ END ]");
	}
}

void xJsonPrintToken(char * pcBuf, jsmntok_t * psT) {
	if (psT->type != JSMN_UNDEFINED || psT->start || psT->end || psT->size) {
		printfx("t=%d/%s  i=%d  s=%d  e=%d  l=%d  '%.*s'\r\n", psT->type, tokType[psT->type],psT->size,
			psT->start, psT->end, psT->end - psT->start, psT->end - psT->start, pcBuf + psT->start);
	} else {
		printfx("[ END ]");
	}
}

void xJsonPrintCurTok(parse_hdlr_t * psPH, const char * pLabel) {
	if (pLabel == NULL)
		pLabel = strNUL;
	if (psPH->jtI < psPH->NumTok) {
		jsmntok_t * psT = &psPH->psTList[psPH->jtI];
		printfx("%s#%d/%d  ", pLabel, psPH->jtI, psPH->NumTok);
		xJsonPrintToken(psPH->pcBuf, psT);
	} else {
		printfx("%s%d=END\r\n", pLabel, psPH->NumTok);
	}
}

void xJsonPrintIndent(int Depth, int Sep, int CR0, int CR1) {
	if (CR0) printfx(strCRLF);
	for (int x = 0; x < Depth; printfx("  "), ++x);
	if (Sep) printfx(" %c", Sep);
	if (CR0) printfx("(s=%d)", CR0);
	if (CR1) printfx(strCRLF);
}

/**
 * Iterates through the token list and prints all tokens, type, size and actual value
 * @param pcBuf
 * @param psT
 * @param Count
 * @param Depth
 * @return
 */
int xJsonPrintTokens(char * pcBuf, jsmntok_t * psT, size_t Count, int Depth) {
	if (Count == 0) {
		return erSUCCESS;
	}
	if (psT->type == JSMN_PRIMITIVE || psT->type == JSMN_STRING) {
		printfx("%d/%s='%.*s'", psT->type, tokType[psT->type], psT->end - psT->start, pcBuf+psT->start);
		return 1;

	} else if (psT->type == JSMN_OBJECT) {
		xJsonPrintIndent(Depth, CHR_L_CURLY, psT->size, 1);
		int j = 0;
		for (int i = 0; i < psT->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0);
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1);
			printfx(" : ");
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1);
			printfx(strCRLF);
		}
		xJsonPrintIndent(Depth, CHR_R_CURLY, 0, 0);
		return j + 1;

	} else if (psT->type == JSMN_ARRAY) {
		xJsonPrintIndent(Depth, CHR_L_SQUARE, psT->size, 1);
		int j = 0;
		for (int i = 0; i < psT->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0);
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1);
			printfx(strCRLF);
		}
		xJsonPrintIndent(Depth, CHR_R_SQUARE, 0, 0);
		return j + 1;
	}
	printfx(strCRLF);
	return 0;
}

/**
 * Invokes parser to determines # of tokens, allocate memory and recalls parser with buffer
 * @param	pBuf
 * @param	xLen
 * @param	pParser
 * @param	ppTokenList
 * @return	number of tokens parsed, 0 or less if error
 */
int xJsonParse(char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTL) {
	int iRV1 = 0, iRV2 = 0;
	jsmn_init(pParser);
	*ppTL = NULL;				// default allocation pointer to NULL
	iRV1 = jsmn_parse(pParser, (const char *) pBuf, xLen, NULL, 0);	// count tokens
	if (iRV1 <= 0) {
		IF_P(debugPARSE, "Failed (%d)\r\n", iRV1);
		return iRV1;
	}

	// Add a spare entry at the end to use as a marker.
	*ppTL = (jsmntok_t *) pvRtosMalloc((iRV1 + 1) * sizeof(jsmntok_t));		// alloc buffer
	jsmn_init(pParser);								// Init & do actual parse...
	iRV2 = jsmn_parse(pParser, (const char *) pBuf, xLen, *ppTL, iRV1);
	// Address extra token and fill with recognisable content
	jsmntok_t * psT = *ppTL;
	psT += iRV1;
	psT->type = JSMN_UNDEFINED;
	psT->start = psT->end = psT->size = 0;
	IF_EXEC_4(debugTRACK && ioB1GET(dbgJSONrd), xJsonPrintTokens, pBuf, *ppTL, iRV1, 0);
	if (iRV1 != iRV2) {
		vRtosFree(*ppTL);
		*ppTL = NULL;
		IF_P(debugPARSE, "Failed (%d != %d)\r\n", iRV1, iRV2);
		return iRV1;
	}
	IF_P(debugPARSE, "Passed (%d) parsed OK\r\n", iRV2);
	return iRV2;
}

bool xJsonTokenIsKey(char * pBuf, jsmntok_t * pToken) {
	size_t Sz = (pToken+1)->start - pToken->end;		// calc gap size between current & next tokens
	return memchr(pBuf+pToken->end, CHR_COLON, Sz) ? 1 : 0;
}

int xJsonFindToken(char * pBuf, jsmntok_t * psT, int nTok, const char * pTok, bool Key) {
	size_t tokLen = strlen(pTok);
	for (int curTok = 0; curTok < nTok; ++curTok, ++psT) {
		size_t curLen = psT->end - psT->start;
		// check for same length & exact content
		if (tokLen != curLen || memcmp(pTok, (char*)pBuf + psT->start, tokLen))
			continue;									// non-matching length or content....
		if (Key && xJsonTokenIsKey(pBuf, psT))			// matching but key required, this a key?
			return ++curTok;							// bingo!!!
	}
	return erFAILURE;
}

int xJsonFindKeyValue(char * pBuf, jsmntok_t * psT, int nTok, const char * pK, const char * pV) {
	IF_EXEC_2(debugTRACK && ioB1GET(dbgJSONrd), xJsonPrintToken, pBuf, psT);
	int iRV = xJsonFindToken(pBuf, psT, nTok, pK, 1);	// Step 1: Find the required Key
	if (iRV < erSUCCESS)
		return iRV;
	psT += iRV;											// Step 2: ensure Value match
	IF_EXEC_2(debugTRACK && ioB1GET(dbgJSONrd), xJsonPrintToken, pBuf, psT);
	size_t szT = psT->end - psT->start;
	size_t szV = strlen(pV);
//	PL("szT=%d (%.*s) szV=%d (%s)", szT, psT->end-psT->start, pBuf + psT->start, szV, pV);
	if ((szT != szV) || memcmp(pBuf + psT->start, pV, szV))
		return erFAILURE;
	++iRV;
	IF_EXEC_2(debugTRACK && ioB1GET(dbgJSONrd), xJsonPrintToken, pBuf, psT);
	return iRV;
}

/**
 * @brief	Primarily used by HTTP requests to pare response values...
 */
int xJsonParseKeyValue(char * pBuf, jsmntok_t * psT, int nTok, const char * pK, px_t pX, cvi_e cvI) {
	IF_EXEC_2(debugTRACK && ioB1GET(dbgJSONrd), xJsonPrintToken, pBuf, psT);
//	xJsonTrackToken(pBuf, psT);
	int iRV = xJsonFindToken(pBuf, psT, nTok, pK, 1);	// locate the Key
	if (iRV < erSUCCESS)
		return iRV;
	psT += iRV;									// step to "value" after matching token..
	IF_EXEC_2(debugTRACK && ioB1GET(dbgJSONrd), xJsonPrintToken, pBuf, psT);
//	xJsonTrackToken(pBuf, psT);
	char * pSrc = (char *) pBuf + psT->start;
	if (cvI == cvSXX) {
		IF_myASSERT(debugPARAM, psT->type == JSMN_STRING);
		size_t szV = psT->end - psT->start;			// calculate length
		strncpy(pX.pc8, pSrc, szV);					// strncpy with exact size,
		*(pX.pc8 + szV) = CHR_NUL;					// terminate
		++iRV;
	} else {
		IF_myASSERT(debugPARAM, psT->type == JSMN_PRIMITIVE);
		iRV = (cvParseValue(pSrc, cvI, pX) == pcFAILURE) ? erFAILURE : iRV + 1;
	}
	return iRV;
}
