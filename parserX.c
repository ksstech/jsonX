/*
 * parserX.c
 * Copyright 2014-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 *
 */

#include "hal_variables.h"
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

// ####################################### Global Functions ########################################

void xJsonPrintCurTok(parse_hdlr_t * psPH) {
	jsmntok_t * psT = &psPH->psTList[psPH->jtI];
	printf("#%d/%d  type=%d  size=%d  beg=%d  end=%d  '%.*s'\r\n", psPH->jtI, psPH->NumTok, psT->type,
		psT->size, psT->start, psT->end, psT->end - psT->start, psPH->pcBuf + psT->start);
}

void xJsonPrintIndent(int Depth, int Sep, int CR0, int CR1) {
	if (CR0) printf(strCRLF);
	for (int x = 0; x < Depth; ++x) printf("  ");
	if (Sep) printf(" %c", Sep);
	if (CR0) printf("(s=%d)", CR0);
	if (CR1) printf(strCRLF);
}

/**
 * Iterates through the token list and prints all tokens, type, size and actual value
 * @param pcBuf
 * @param psT
 * @param Count
 * @param Depth
 * @return
 */
int xJsonPrintTokens(const char * pcBuf, jsmntok_t * psT, size_t Count, int Depth) {
	if (Count == 0) {
		return erSUCCESS;
	}
	if (psT->type == JSMN_PRIMITIVE || psT->type == JSMN_STRING) {
		printf("%d='%.*s'", psT->type, psT->end - psT->start, pcBuf+psT->start);
		return 1;

	} else if (psT->type == JSMN_OBJECT) {
		xJsonPrintIndent(Depth, CHR_L_CURLY, psT->size, 1);
		int j = 0;
		for (int i = 0; i < psT->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0);
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1);
			printf(" : ");
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1);
			printf(strCRLF);
		}
		xJsonPrintIndent(Depth, CHR_R_CURLY, 0, 0);
		return j + 1;

	} else if (psT->type == JSMN_ARRAY) {
		xJsonPrintIndent(Depth, CHR_L_SQUARE, psT->size, 1);
		int j = 0;
		for (int i = 0; i < psT->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0);
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1);
			printf(strCRLF);
		}
		xJsonPrintIndent(Depth, CHR_R_SQUARE, 0, 0);
		return j + 1;
	}
	printf(strCRLF);
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
int xJsonParse(const char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTokenList) {
	int iRV1 = 0, iRV2 = 0;
	jsmn_init(pParser);
	*ppTokenList = NULL;				// default allocation pointer to NULL
	iRV1 = jsmn_parse(pParser, (const char *) pBuf, xLen, NULL, 0);	// count tokens
	if (iRV1 <= 0) {
		IF_P(debugPARSE, "Failed (%d)\r\n", iRV1);
		return iRV1;
	}

	*ppTokenList = (jsmntok_t *) pvRtosMalloc(iRV1 * sizeof(jsmntok_t));		// alloc buffer
	jsmn_init(pParser);								// Init & do actual parse...
	iRV2 = jsmn_parse(pParser, (const char *) pBuf, xLen, *ppTokenList, iRV1);
	IF_EXEC_4(debugPARSE, xJsonPrintTokens, pBuf, *ppTokenList, iRV1, 0);
	if (iRV1 != iRV2) {
		vRtosFree(*ppTokenList);
		*ppTokenList = NULL;
		IF_P(debugPARSE, "Failed (%d != %d)\r\n", iRV1, iRV2);
		return iRV1;
	}
	IF_P(debugPARSE, "Passed (%d) parsed OK\r\n", iRV2);
	return iRV2;
}

bool xJsonTokenIsKey(const char * pBuf, jsmntok_t * pToken) {
	size_t Sz = (pToken+1)->start - pToken->end;
	return memchr(pBuf+pToken->end, CHR_COLON, Sz) ? 1 : 0;
}

int xJsonFindToken(const char * pBuf, jsmntok_t * psT, int numTok, const char * pK, bool Key) {
	size_t kL = strlen(pK);
	for (int curTok = 0; curTok < numTok; ++curTok, ++psT) {
		size_t tL = psT->end - psT->start;
		// check foe same length & content
		if (kL != tL || !xstrncmp(pK, (char *) pBuf + psT->start, kL, 1))
			continue;
		if (!Key || xJsonTokenIsKey(pBuf, psT))			// key required and found?
			return curTok;								// bingo!!!
	}
	return erFAILURE;
}

int xJsonFindKeyValue(const char * pBuf, jsmntok_t * psT, int NumTok, const char * pK, const char * pV) {
	int iRV = xJsonFindKey(pBuf, psT, NumTok, pK);		// Step 1: Find the required Key
	if (iRV == erFAILURE)
		return erFAILURE;
	jsmntok_t * psTV = psT + (++iRV);					// Step 2: ensure Value match
	if (xstrncmp(pBuf + psTV->start, pV, psTV->end - psTV->start, 1))
		return iRV;
	return erFAILURE;
}

int xJsonParseKeyValue(const char * pBuf, jsmntok_t * pToken, int NumTok, const char * pKey, px_t pX, cvi_e cvI) {
	int iRV = xJsonFindKey(pBuf, pToken, NumTok, pKey);
	if (iRV == erFAILURE)
		return iRV;
	pToken += iRV + 1;									// step to token after matching token..
	char * pSrc = (char *) pBuf + pToken->start;
	IF_myASSERT(debugPARAM, (cvI == cvSXX && pToken->type == JSMN_STRING) ||
							(cvI != cvSXX && pToken->type == JSMN_PRIMITIVE));
	if (pX.pv == NULL)										// if no parse destination supplied
		return iRV;										// just return the token index
	if (cvI == cvSXX) {
		int xLen = pToken->end - pToken->start;			// calculate length
		strncpy(pX.pc8, pSrc, xLen);					// strncpy with exact size,
		*(pX.pc8 + xLen) = CHR_NUL;						// terminate
	} else {
		if (cvParseValue(pSrc, cvI, pX) == pcFAILURE)
			return erFAILURE;
	}
	return iRV;
}
