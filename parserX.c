/*
 * parserX.c - Copyright 2014-24 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 */

#include "hal_platform.h"
#include "hal_options.h"
#include "parserX.h"
#include "printfx.h"									// x_definitions stdarg stdint stdio
#include "syslog.h"
#include "x_errors_events.h"
#include "x_string_general.h"
#include "x_string_to_values.h"

#include <string.h>

// ############################### BUILD: debug configuration options ##############################

#define	debugFLAG					0xF000
#define	debugPARSE					(debugFLAG & 0x0001)
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

const char * tokType[] = { "Undef", "Object", "Array", "", "String", "", "", "", "Value" };

// ####################################### Global Functions ########################################

static int xJsonPrintToken(report_t * psR,  parse_hdlr_t * psPH) {
	IF_myASSERT(debugTRACK, INRANGE(0, psPH->CurTok, psPH->NumTok));
	IF_myASSERT(debugTRACK, INRANGE(psPH->psT0, psPH->psTx, &psPH->psT0[psPH->CurTok]));
	int iRV = 0;
	if (psPH->psTx->start && psPH->psTx->end)
		iRV += wprintfx(psR, "'%.*s' ", psPH->psTx->end - psPH->psTx->start, psPH->pcBuf + psPH->psTx->start);
	if (iRV && psPH->psTx->type && psPH->psTx->size) {
		iRV += wprintfx(psR, "t=%d/%s s=%d b=%d e=%d l=%d", psPH->psTx->type, tokType[psPH->psTx->type],
			psPH->psTx->size, psPH->psTx->start, psPH->psTx->end, psPH->psTx->end - psPH->psTx->start);
	}
	if (iRV)
		iRV += wprintfx(psR, strNL);
	return iRV;
}

static int xJsonPrintIndent(report_t * psR, int Depth, int Sep, int CR0, int CR1) {
	int iRV = 0;
	if (CR0)
		iRV += wprintfx(psR, strNL);
//	iRV += wprintfx(psR, "%*s", Depth * psR->sFM.jsIndent, " ");
	for (int x = 0; x < Depth; ++x) iRV += wprintfx(psR, "  ");
	if (Sep)
		iRV += wprintfx(psR, " %c", Sep);
	if (CR0)
		iRV += wprintfx(psR, "(s=%d)", CR0);
	if (CR1)
		iRV += wprintfx(psR, strNL);
	return iRV;
}

void xJsonPrintCurTok(parse_hdlr_t * psPH, const char * pLabel) {
	report_t sRprt = { .sFM.u32Val = makeMASK08_3x8(0,0,0,0,0,0,0,0,2,0,0) };
	psPH->psTx = &psPH->psT0[psPH->CurTok];
	wprintfx(&sRprt, "%s#%d/%d  ", pLabel ? pLabel : strNUL, psPH->CurTok, psPH->NumTok);
	xJsonPrintToken(&sRprt, psPH);
}

/**
 * Iterates through the token list and prints all tokens, type, size and actual value
 * @param psP
 * @param psPH
 * @param Depth
 * @return
 */
static int xJsonReportTokensRecurse(report_t * psR, parse_hdlr_t * psPH, jsmntok_t * pasTL, int Count, int Depth) {
	IF_myASSERT(debugTRACK, psR);
//	if (Count == 0) return 0;
	int iRV;
	if (pasTL->type == JSMN_PRIMITIVE || pasTL->type == JSMN_STRING) {
		wprintfx(psR, "%d/%s='%.*s'", pasTL->type, tokType[pasTL->type], pasTL->end - pasTL->start, psPH->pcBuf + pasTL->start);
		iRV = 1;
	} else if (pasTL->type == JSMN_OBJECT) {
		if (repFLAG_TST(psR,sFM.jsIndent))
			xJsonPrintIndent(psR, Depth, CHR_L_CURLY, pasTL->size, 1);
		int j = 0;
		for (int i = 0; i < pasTL->size; ++i) {
			if (repFLAG_TST(psR,sFM.jsIndent))
				xJsonPrintIndent(psR, Depth+2, 0, 0, 0);
			j += xJsonReportTokensRecurse(psR, psPH, pasTL+j+1, Count-j, Depth+1);
			wprintfx(psR, " : ");
			j += xJsonReportTokensRecurse(psR, psPH, pasTL+j+1, Count-j, Depth+1);
			wprintfx(psR, strNL);
		}
		if (repFLAG_TST(psR,sFM.jsIndent))
			xJsonPrintIndent(psR, Depth, CHR_R_CURLY, 0, 0);
		iRV = j + 1;
	} else if (pasTL->type == JSMN_ARRAY) {
		if (repFLAG_TST(psR,sFM.jsIndent))
			xJsonPrintIndent(psR, Depth, CHR_L_SQUARE, pasTL->size, 1);
		int j = 0;
		for (int i = 0; i < pasTL->size; ++i) {
			if (repFLAG_TST(psR,sFM.jsIndent))
				xJsonPrintIndent(psR, Depth+2, 0, 0, 0);
			j += xJsonReportTokensRecurse(psR, psPH, pasTL+j+1, Count-j, Depth+1);
			wprintfx(psR, strNL);
		}
		if (repFLAG_TST(psR,sFM.jsIndent))
			xJsonPrintIndent(psR, Depth, CHR_R_SQUARE, 0, 0);
		iRV = j + 1;
	} else {
		iRV = 0;
		wprintfx(psR, strNL);
	}
	return iRV;
}

int xJsonReportTokens(parse_hdlr_t * psPH, int Depth) {
	report_t sRprt = { .sFM.u32Val = makeMASK08_3x8(0,0,0,0,0,0,0,0,2,0,0) };
	psPH->CurTok = 0;
	psPH->psTx = &psPH->psT0[psPH->CurTok];
	return xJsonReportTokensRecurse(&sRprt, psPH, psPH->psTx, 0, Depth);
}

/**
 * @brief	parser to determines # of tokens, allocate memory and recalls parser with buffer
 * @param	pBuf
 * @param	xLen
 * @param	pParser
 * @param	ppTokenList
 * @return	number of tokens parsed, 0 or less if error
 */
int xJsonParse(parse_hdlr_t * psPH) {
	#define jsonEXTRA_SIZE	5
	// count tokens to determine buffer to be allocated
	jsmn_init(&psPH->sParser);
	int iRV = jsmn_parse(&psPH->sParser, psPH->pcBuf, psPH->szBuf, NULL, 0);
	if (iRV > 0) {
		psPH->NumTok = iRV;
		// Allocate memory, add spare space at the end (all ZEROS ie JSMN_UNDEFINED)
		psPH->psT0 = (jsmntok_t *) calloc(iRV+jsonEXTRA_SIZE, sizeof(jsmntok_t));
		IF_myASSERT(debugRESULT, psPH->psT0);
		// perform the actual parsing process
		jsmn_init(&psPH->sParser);
		iRV = jsmn_parse(&psPH->sParser, psPH->pcBuf, psPH->szBuf, psPH->psT0, iRV+jsonEXTRA_SIZE);
		if (psPH->NumTok != iRV)
			SL_ERR("Incomplete parsing %d vs %d", iRV, psPH->NumTok);
		IF_EXEC_2(debugTRACK && ioB2GET(dbgJSONrd)==3, xJsonReportTokens, psPH, 0);
	}
	return iRV;
}

/**
 * @brief	Find a specific token in the buffer
 * @return	Value > 0 (the NEXT token index) if found else erFAILURE
 */
int xJsonFindToken(parse_hdlr_t * psPH, const char * pTok, int xKey) {
	size_t tokLen = strlen(pTok);
//	IF_PX(debugTRACK && allSYSFLAGS(sfTRACKER), "Find '%s'(%d) (%s)\r\n", pTok, tokLen, xKey ? "KEY" : "token");
	// Ensure we are starting at current indexed token...
	psPH->psTx = &psPH->psT0[psPH->CurTok];
	for (psPH->CurTok = 0; psPH->CurTok < psPH->NumTok; ++psPH->CurTok) {
		psPH->psTx = &psPH->psT0[psPH->CurTok];
		size_t curLen = psPH->psTx->end - psPH->psTx->start;
		// Fix to avoid crash if full object not received, ie xJsonParse 2 phase returned different results
		if (curLen > psPH->szBuf)
			goto next;
//		IF_PX(debugTRACK && allSYSFLAGS(sfTRACKER), "T#%d/%d  %d->%d=%d '%.*s'\r\n", psPH->CurTok, psPH->NumTok, psPH->psTx->start, psPH->psTx->end, psPH->psTx->end-psPH->psTx->start, curLen, psPH->pcBuf+psPH->psTx->start);
		// check for same length & exact content
		if ((tokLen == curLen) &&								// check length
			(memcmp(pTok, psPH->pcBuf + psPH->psTx->start, curLen) == 0)) {	// length OK, check content
			if (xKey != 0) {
				jsmntok_t * pTokNxt = (jsmntok_t*) ((void *) psPH->psTx + sizeof(jsmntok_t));
				size_t GapLen = pTokNxt->start - psPH->psTx->end;
				// Now check if ':' present in characters between the 2 tokens...
				void * pV = memchr(psPH->pcBuf+psPH->psTx->end, CHR_COLON, GapLen);
//				IF_PX(debugTRACK && allSYSFLAGS(sfTRACKER), " %p `%.*s`", pV, GapLen, psPH->pcBuf+psPH->psTx->end);
				if (pV == NULL) {
//					IF_PX(debugTRACK && allSYSFLAGS(sfTRACKER), " %d=not KEY", psPH->CurTok);
					goto next;
				}
			}
//			IF_PX(debugTRACK && allSYSFLAGS(sfTRACKER), " [Found]" strNL);
			++psPH->CurTok;								// Index to value after "key : "
			psPH->psTx = &psPH->psT0[psPH->CurTok];		// and set pointer the same...
			return psPH->CurTok;
		}
next:
	}
//	IF_PX(debugTRACK && allSYSFLAGS(sfTRACKER), " [NOT FOUND]" strNL);
	psPH->CurTok = 0;
	psPH->psTx = NULL;
	return erFAILURE;
}

int xJsonFindKeyValue(parse_hdlr_t * psPH, const char * pK, const char * pV) {
	IF_EXEC_2(debugTRACK && ioB2GET(dbgJSONrd)&1, xJsonPrintToken, NULL, psPH);
	int iRV = xJsonFindToken(psPH, pK, 1);	// Step 1: Find the required Key
	if (iRV < erSUCCESS)
		return iRV;
	// at this stage psPH members are setup based on token found....
	IF_EXEC_2(debugTRACK && ioB2GET(dbgJSONrd)&1, xJsonPrintToken, NULL, psPH);
	size_t szT = psPH->psTx->end - psPH->psTx->start;
	size_t szV = strlen(pV);
	if ((szT != szV) || memcmp(psPH->pcBuf + psPH->psTx->start, pV, szV) != 0) {
		psPH->CurTok = 0;
		psPH->psTx = NULL;
		return erFAILURE;
	}
	++psPH->CurTok;								// Index to value after "key : "
	psPH->psTx = &psPH->psT0[psPH->CurTok];		// and set pointer the same...
	IF_EXEC_2(debugTRACK && ioB2GET(dbgJSONrd)&1, xJsonPrintToken, NULL, psPH);
	return iRV;
}

/**
 * @brief	Primarily used by HTTP requests to parse response values to variable locations
 * @return	0 if token not found or error parsing else 1
 */
int xJsonParseEntry(parse_hdlr_t * psPH, ph_entry_t * psEntry) {
//	IF_PX(debugTRACK && allSYSFLAGS(sfTRACKER), "[%s/%s] ", pcIndex2String(psEntry->cvI), psEntry->pcKey);
	int iRV = xJsonFindToken(psPH, psEntry->pcKey, 1);
	if (iRV <= erSUCCESS)
		return 0;
	// if successful, structure members already updates for token found...
	IF_EXEC_2(debugTRACK && ioB2GET(dbgJSONrd)&1, xJsonPrintToken, NULL, psPH);
	char * pSrc = (char *) psPH->pcBuf + psPH->psTx->start;
	if (psEntry->pxVar.pv != NULL) {
		if (psEntry->cvI == cvSXX) {
			IF_myASSERT(debugTRACK, psPH->psTx->type == JSMN_STRING);
			size_t szV = psPH->psTx->end - psPH->psTx->start;				// calculate length
			strncpy(psEntry->pxVar.pc8, pSrc, szV);			// strncpy with exact size,
			*(psEntry->pxVar.pc8 + szV) = CHR_NUL;			// terminate
		} else {
			IF_myASSERT(debugTRACK, psPH->psTx->type == JSMN_PRIMITIVE);
			if (cvParseValue(pSrc, psEntry->cvI, psEntry->pxVar) == pcFAILURE) {
				return 0;
			}
		}
	}
	return 1;
}
