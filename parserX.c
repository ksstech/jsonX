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
#include "hal_network.h"
#include "parserX.h"
#include "printfx.h"									// x_definitions stdarg stdint stdio
#include "syslog.h"
#include "x_errors_events.h"
#include "x_string_general.h"
#include "x_string_to_values.h"

// ############################### BUILD: debug configuration options ##############################

#define	debugFLAG					0xF000

#define	debugLIST					(debugFLAG & 0x0002)
#define	debugPARSE					(debugFLAG & 0x0004)
#define	debugARRAY					(debugFLAG & 0x0008)

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
	if (CR0)
		printf(strCRLF);
	for (int x = 0; x < Depth; ++x)
		printf("  ");
	if (Sep)
		printf(" %c", Sep);
	if (CR0)
		printf("(s=%d)", CR0);
	if (CR1)
		printf(strCRLF);
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
	else
		++iRV;											// step to next token
	// Step 2: ensure Value match
	jsmntok_t * pTV = pToken + iRV;
	PX("V=%s vs %.*s", pV, pToken->end - pToken->start, pBuf + pToken->start);
	if (xstrncmp(pBuf+pTV->start, pV, pTV->end-pTV->start, 1))
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

/**
 * @brief	Parse JSON array for strings or primitives, store directly into empty db_t structure record
 * @brief	Expect jtI to be the token# of the array to be parsed.
 * @param	psPH - Pointer to pre-initialised parser handler structure
 * @param	paDst - Pointer to array of pointers for each element
 * @param	Count - Maximum number of elements to parse
 * @param	paDBF[] - pointer to array of dbf_t structures defining each db_t record
 * @return	Number of elements parsed or erFAILURE
 * 			if successful, leaves jtI set to next token to parse
 * 			if failed, jti left at the failing element
 */
int xJsonParseArrayDB(parse_hdlr_t * psPH, px_t paDst[], int szArr, dbf_t paDBF[]) {
	if (psPH->psTList[psPH->jtI].size != szArr) {
		IF_P(debugARRAY, "Invalid Array size (%u) or count(%u)\r\n", psPH->psTList[psPH->jtI].size, szArr);
		return erFAILURE;
	}
	int NumOK = 0;
	jsmntok_t * psT = &psPH->psTList[++psPH->jtI];		// step into ARRAY to 1st ELEMENT
	for (int i = 0; i < szArr; ++psT, ++i) {
		vf_e cvF = xIndex2Form(paDBF[i].cvI);
		char * pcBuf = (char *) psPH->pcBuf + psT->start;
		char * pSaved = (char *) psPH->pcBuf + psT->end;
		char cSaved = *pSaved;							// Save char before overwrite
		*pSaved = CHR_NUL;								// terminate
		if (psT->type == JSMN_PRIMITIVE && cvF != vfSXX) {
			if (*pcBuf == CHR_n || *pcBuf == CHR_f) {
				pcBuf[0] = CHR_0;						// default 'null' & 'false' to 0
				pcBuf[1] = CHR_NUL;
			} else if (*pcBuf == CHR_t) {
				pcBuf[0] = CHR_1;						// default 'true' to 1
				pcBuf[1] = CHR_NUL;
			}
			if (cvParseValue(pcBuf, paDBF[i].cvI, paDst[i]) == pcFAILURE) {
				*pSaved = cSaved;
				IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH);
				return erFAILURE;
			}
			++psPH->jtI;
			++NumOK;
		} else if (psT->type == JSMN_STRING && cvF == vfSXX) {
			uint8_t szTok = psT->end - psT->start;
			memcpy(paDst[i].pv, pcBuf, szTok < paDBF[i].fS ? szTok : paDBF[i].fS);
			++psPH->jtI;
			++NumOK;
		} else {
			IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH);
			return erFAILURE;
		}
		*pSaved = cSaved;
	}
	return NumOK;
}

/**
 * Parse JSON array for strings or primitives
 * @brief	EXPECT jtI to be the token# of the array to be parsed.
 * @param	psPH	Pointer to pre-initialised parser handler structure
 * @param	pDst	Pointer to storage array for "primitive" elements (CHECK SIZE !!!)
 * @param	Hdlr	Handler function to process string elements
 * @param	Count	Maximum number of elements to parse
 * @param	cvF		Format of primitive elements
 * @param	cvS		Size of primitive element values
 * @return	Number of elements parsed or erFAILURE
 * 			if successful, leaves jtI set to next token to parse
 * 			if failed, jti left at the failing element
 */
int xJsonParseArray(parse_hdlr_t * psPH, px_t pDst, int(* Hdlr)(char *), int szArr, cvi_e cvI) {
	IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH);
	if (szArr < 1 || psPH->psTList[psPH->jtI].size != szArr) {
		IF_P(debugARRAY, "Invalid Array size (%u) or count(%u)\r\n", psPH->psTList[psPH->jtI].size, szArr);
		return erFAILURE;
	}
	int NumOK = 0;
	vf_e cvF = xIndex2Form(cvI);
	jsmntok_t * psT = &psPH->psTList[++psPH->jtI];		// step to first ARRAY ELEMENT
	for (int i = 0; i < szArr; ++psT, ++i) {
		char * pcBuf = (char *) psPH->pcBuf + psT->start;
		char * pSaved = (char *) psPH->pcBuf + psT->end;
		char cSaved = *pSaved;							// Save char before overwrite
		*pSaved = CHR_NUL;								// terminate
		if ((psT->type == JSMN_PRIMITIVE) && (cvF != vfSXX) && pDst.pu8) {
			if (*pcBuf == CHR_n || *pcBuf == CHR_f) {
				pcBuf[0] = CHR_0;						// default 'null' & 'false' to 0
				pcBuf[1] = CHR_NUL;
			} else if (*pcBuf == CHR_t) {
				pcBuf[0] = CHR_1;						// default 'true' to 1
				pcBuf[1] = CHR_NUL;
			}
			if (cvParseValue(pcBuf, cvI, pDst) == pvFAILURE) {
				*pSaved = cSaved;
				return erFAILURE;
			}
			pDst.pu8 += xIndex2Field(cvI);
		} else if ((psT->type == JSMN_STRING) && (cvF == vfSXX) && Hdlr) {
			int iRV = Hdlr(pcBuf);
			if (iRV < erSUCCESS) {
				*pSaved = cSaved;
				return iRV;
			}
		} else {
			return erFAILURE;
		}
		++NumOK;
		++psPH->jtI;
		*pSaved = cSaved;
	}
	return NumOK;
}

/* parse the buffer
 * find token "method"
 * {"method":"sense","params":{"req":[1,1,2],"sval":["sense /idi 1 0 3;sense /idi 2 0 3;sense /idi 3 0 3;sense /idi 4 0 0;sense /idi 5 0 -1","sense /clock/uptime 1000 60000"]}}
 * {"method":"rule","params":{"req":[1,1,1,1],"rval":["sch IP4B IF HR GE 1 AND HR LT 4 then upgrade"]}}
 */
int	xJsonParsePayload(parse_hdlr_t * psPH, const ph_list_t * psHL, size_t szHL) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(psHL) && szHL > 0);
	int iRV = xJsonParse(psPH->pcBuf, psPH->szBuf, &psPH->sParser, &psPH->psTList);
	if (iRV < 1 || psPH->psTList == NULL)
		goto exit;
	psPH->NumTok = iRV;

	// Check if not a "status" message
	iRV = xJsonFindKey(psPH->pcBuf, psPH->psTList, psPH->NumTok, "status");
	if (iRV >= erSUCCESS)
		goto exit;										// just ignore

		// "method" is what we REALLY should be getting...
		iRV = xJsonFindKey(psPH->pcBuf, psPH->psTList, psPH->NumTok, "method");
		if (iRV < erSUCCESS)
			goto exit;
		psPH->jtI = ++iRV;	// step over 'method'

	while(psPH->jtI < psPH->NumTok) {					// Inside jsmntok loop
		jsmntok_t * psT = &psPH->psTList[psPH->jtI];
//		RPL("V1='%s'", psPH->pcBuf+psT->start);
		for (int hlI = 0; hlI < szHL; ++hlI) {			// loop PARSE HANDLER LIST
			const ph_list_t * psH = &psHL[hlI];
//			RP("  V2='%s'", psH->pToken);
			if (xstrncmp(psPH->pcBuf + psT->start, psH->pToken, strlen(psH->pToken), true)) {
				++psPH->jtI;
				iRV = psH->pHdlr(psPH);					// invoke CB handler
				if (iRV >= erSUCCESS) {
					psPH->NumOK += iRV;
				} else {
					SL_CRIT("WTF: '%.*s'", psT->end - psT->start, psPH->pcBuf + psT->start);
				}
				break;
			} else {
				// try next hlI entry in the list
			}
		} // all hlI entries tried.
		}
	}
	IF_EXEC_4(debugLIST && iRV >= erSUCCESS, xJsonPrintTokens, psPH->pcBuf, psPH->psTList, psPH->NumTok, 0);
exit:
	vRtosFree(psPH->psTList);
    return psPH->NumOK ? psPH->NumOK : iRV;
}

// Old API replaced with above...
int	xJsonParseList(const ph_list_t * psPlist, size_t szPList, const char * pcBuf, size_t szBuf, void * pvArg) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(psPlist) && szPList > 0);
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pcBuf) && szBuf > 0);
	parse_hdlr_t sPH = { 0 };
	int iRV = 0;
	iRV = xJsonParse(pcBuf, szBuf, &sPH.sParser, &sPH.psTList);
	if (iRV < 1 || sPH.psTList == NULL) {
		IF_P(debugRESULT, "jsmnX parse error (%d)\r\n", iRV);
		return erFAILURE;
	}
	sPH.pcBuf	= pcBuf;
	sPH.szBuf	= szBuf;
	sPH.pvArg	= pvArg;
	sPH.NumTok	= iRV;
	for (int hlI = 0; hlI < szPList; ++hlI) {			// Outside parse_list_t loop
		char * pcKey = psPlist[hlI].pToken;
		size_t szKey = strlen(pcKey);
		sPH.jtI	= 0;
		while(sPH.jtI < sPH.NumTok) {					// Inside jsmntok loop
			jsmntok_t * psT = &sPH.psTList[sPH.jtI];
			if ((szKey == (psT->end - psT->start)) &&
				(xstrncmp(pcKey, pcBuf+psT->start, szKey,1) == 1)) {
				IF_EXEC_1(debugLIST, xJsonPrintCurTok, &sPH);
				++sPH.jtI;								// ensure start at next
				iRV = psPlist[hlI].pHdlr(&sPH);			// invoke CB handler
				if (iRV >= erSUCCESS) {
					sPH.NumOK += iRV;
				}
			} else {
				++sPH.jtI;								// skip this token
			}
		}
	}
	IF_EXEC_4(debugLIST && iRV >= erSUCCESS, xJsonPrintTokens, pcBuf, sPH.psTList, sPH.NumTok, 0);
	vRtosFree(sPH.psTList);
    return sPH.NumOK ? sPH.NumOK : iRV;
}
