/*
 * parserX.c
 * Copyright 2014-21 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 *
 */

#include	"hal_variables.h"
#include	"parserX.h"

#include	"x_string_general.h"
#include	"x_string_to_values.h"

#include	"printfx.h"									// x_definitions stdarg stdint stdio
#include	"x_errors_events.h"

// ############################### BUILD: debug configuration options ##############################

#define	debugFLAG					0xF000

#define	debugFINDKEY				(debugFLAG & 0x0001)
#define	debugLIST					(debugFLAG & 0x0002)
#define	debugPARSE					(debugFLAG & 0x0004)
#define	debugARRAY					(debugFLAG & 0x0008)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ####################################### Global Functions ########################################

void xJsonPrintCurTok(parse_hdlr_t * psPH) {
	jsmntok_t * psT = &psPH->psTList[psPH->jtI] ;
	printfx("#%d/%d  t=%d  s=%d  b=%d  e=%d  '%.*s'\r\n", psPH->jtI, psPH->NumTok, psT->type,
		psT->size, psT->start, psT->end, psT->end - psT->start, psPH->pcBuf + psT->start) ;
}

void xJsonPrintIndent(int Depth, int Sep, int CR0, int CR1) {
	if (CR0)
		printfx(strCRLF);
	for (int x = 0; x < Depth; ++x)
		printfx("  ");
	if (Sep)
		printfx(" %c", Sep);
	if (CR0)
		printfx("(s=%d)", CR0);
	if (CR1)
		printfx(strCRLF);
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
		return erSUCCESS ;
	}
	if (psT->type == JSMN_PRIMITIVE || psT->type == JSMN_STRING) {
		printfx("%d='%.*s'", psT->type, psT->end - psT->start, pcBuf+psT->start) ;
		return 1 ;

	} else if (psT->type == JSMN_OBJECT) {
		xJsonPrintIndent(Depth, CHR_L_CURLY, psT->size, 1) ;
		int j = 0 ;
		for (int i = 0; i < psT->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0) ;
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1) ;
			printfx(" : ") ;
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1) ;
			printfx(strCRLF);
		}
		xJsonPrintIndent(Depth, CHR_R_CURLY, 0, 0) ;
		return j + 1 ;

	} else if (psT->type == JSMN_ARRAY) {
		xJsonPrintIndent(Depth, CHR_L_SQUARE, psT->size, 1) ;
		int j = 0 ;
		for (int i = 0; i < psT->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0) ;
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1) ;
			printfx(strCRLF) ;
		}
		xJsonPrintIndent(Depth, CHR_R_SQUARE, 0, 0) ;
		return j + 1 ;
	}
	printfx(strCRLF) ;
	return 0 ;
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
	int iRV1 = 0, iRV2 = 0 ;
	jsmn_init(pParser) ;
	*ppTokenList = NULL ;				// default allocation pointer to NULL
	iRV1 = jsmn_parse(pParser, (const char *) pBuf, xLen, NULL, 0) ;	// count tokens
	if (iRV1 <= 0) {
		IF_P(debugPARSE, "Failed (%d)\r\n", iRV1) ;
		return iRV1 ;
	}

	*ppTokenList = (jsmntok_t *) pvRtosMalloc(iRV1 * sizeof(jsmntok_t)) ;		// alloc buffer
	jsmn_init(pParser) ;								// Init & do actual parse...
	iRV2 = jsmn_parse(pParser, (const char *) pBuf, xLen, *ppTokenList, iRV1) ;
	IF_EXEC_4(debugPARSE, xJsonPrintTokens, pBuf, *ppTokenList, iRV1, 0) ;
	if (iRV1 != iRV2) {
		vRtosFree(*ppTokenList) ;
		*ppTokenList = NULL ;
		IF_P(debugPARSE, "Failed (%d != %d)\r\n", iRV1, iRV2) ;
		return iRV1 ;
	}
	IF_P(debugPARSE, "Passed (%d) parsed OK\r\n", iRV2) ;
	return iRV2 ;
}

int xJsonCompareKey(const char * pKey, int TokLen, char * pTok) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(pKey) && halCONFIG_inMEM(pTok) && TokLen > 0) ;
	while (*pKey && *pTok && TokLen) {
		if (toupper((int)*pKey) != toupper((int)*pTok))
			return erFAILURE;
		++pKey ;
		++pTok ;
		--TokLen ;
	}
	return (*pKey == 0 && TokLen == 0) ? erSUCCESS : erFAILURE ;
}

int xJsonFindKey(const char * pBuf, jsmntok_t * pToken, int NumTok, const char * pKey) {
	IF_P(debugFINDKEY,"\r\n%s: Key='%s'\r\n", __FUNCTION__, pKey) ;
	size_t KeyLen = strlen(pKey);
	for (int CurTok = 0; CurTok < NumTok; CurTok++, pToken++) {
		if (KeyLen != (pToken->end - pToken->start)) {
			continue ;							// not same length, skip
		}
		IF_P(debugFINDKEY, "[Tok='%.*s'] ", pToken->end - pToken->start, pBuf + pToken->start) ;
		if (xJsonCompareKey(pKey, KeyLen, (char *) pBuf + pToken->start) == erFAILURE) {
			continue ;							// not matching, skip
		}
		IF_P(debugFINDKEY, strCRLF) ;
		return CurTok ;							// all OK, return token number
	}
	IF_P(debugFINDKEY, strCRLF) ;
	return erFAILURE ;
}

/**
 * @brief
 * @param	pBuf
 * @param	pToken
 * @param	NumTok
 * @param	pKey
 * @param	pValue
 * @param	cvF
 * @return	erSUCCESS or erFAILURE
 */
int xJsonParseKeyValue(const char * pBuf, jsmntok_t * pToken, int NumTok, const char * pKey, px_t pX, cvi_e cvI) {
	int iRV = xJsonFindKey(pBuf, pToken, NumTok, pKey);
	if (iRV == erFAILURE)
		return iRV;
	pToken += iRV + 1 ;									// step to token after matching token..
	char * pSrc = (char *) pBuf + pToken->start;
	if (cvI == cvSXX) {
		IF_myASSERT(debugPARAM, pToken->type == JSMN_STRING) ;
		int xLen = pToken->end - pToken->start ;			// calculate length
		strncpy(pX.pc8, pSrc, xLen) ;
		*(pX.pc8 + xLen) = CHR_NUL ;						// strncpy with exact size, no termination..
		iRV = erSUCCESS ;
	} else {
		IF_myASSERT(debugPARAM, pToken->type == JSMN_PRIMITIVE);
		char * pTmp = cvParseValue(pSrc, cvI, pX);
		iRV = (pTmp == pcFAILURE) ? erFAILURE : erSUCCESS;
	}
	return iRV ;
}

/**
 * Parse JSON array for strings or primitives, store directly into empty db_t structure record
 * @brief	EXPECT jtI to be the token# of the array to be parsed.
 * @param	psPH	Pointer to pre-initialised parser handler structure
 * @param	paDst	Pointer to array of pointers for each element
 * @param	Count	Maximum number of elements to parse
 * @param	paDBF[]	pointer to array of dbf_t structures defining each db_t record
 * @return	Number of elements parsed or erFAILURE
 * 			if successful, leaves jtI set to next token to parse
 * 			if failed, jti left at the failing element
 */
int xJsonParseArrayDB(parse_hdlr_t * psPH, px_t paDst[], int szArr, dbf_t paDBF[]) {
	if (psPH->psTList[psPH->jtI].size != szArr) {
		IF_P(debugARRAY, "Invalid Array size (%u) or count(%u)\r\n", psPH->psTList[psPH->jtI].size, szArr) ;
		return erFAILURE ;
	}
	int NumOK = 0 ;
	jsmntok_t * psT = &psPH->psTList[++psPH->jtI] ;		// step into ARRAY to 1st ELEMENT
	for (int i = 0; i < szArr; ++psT, ++i) {
		vf_e cvF = xIndex2Form(paDBF[i].cvI) ;
		char * pcBuf = (char *) psPH->pcBuf + psT->start ;
		char * pSaved = (char *) psPH->pcBuf + psT->end ;
		char cSaved = *pSaved ;							// Save char before overwrite
		*pSaved = CHR_NUL ;								// terminate
		if (psT->type == JSMN_PRIMITIVE && cvF != vfSXX) {
			if (*pcBuf == CHR_n || *pcBuf == CHR_f) {
				pcBuf[0] = CHR_0 ;						// default 'null' & 'false' to 0
				pcBuf[1] = CHR_NUL ;
			} else if (*pcBuf == CHR_t) {
				pcBuf[0] = CHR_1 ;						// default 'true' to 1
				pcBuf[1] = CHR_NUL ;
			}
			if (cvParseValue(pcBuf, paDBF[i].cvI, paDst[i]) == pcFAILURE) {
				*pSaved = cSaved ;
				IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH) ;
				return erFAILURE ;
			}
			++psPH->jtI ;
			++NumOK ;
		} else if (psT->type == JSMN_STRING && cvF == vfSXX) {
			uint8_t szTok = psT->end - psT->start ;
			memcpy(paDst[i].pv, pcBuf, szTok < paDBF[i].fS ? szTok : paDBF[i].fS) ;
			++psPH->jtI ;
			++NumOK ;
		} else {
			IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH) ;
			return erFAILURE ;
		}
		*pSaved = cSaved ;
	}
	return NumOK ;
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
	IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH) ;
	if (szArr < 1 || psPH->psTList[psPH->jtI].size != szArr) {
		IF_P(debugARRAY, "Invalid Array size (%u) or count(%u)\r\n", psPH->psTList[psPH->jtI].size, szArr) ;
		return erFAILURE ;
	}
	int NumOK = 0 ;
	vf_e cvF = xIndex2Form(cvI);
	jsmntok_t * psT = &psPH->psTList[++psPH->jtI] ;		// step to first ARRAY ELEMENT
	for (int i = 0; i < szArr; ++psT, ++i) {
		char * pcBuf = (char *) psPH->pcBuf + psT->start ;
		char * pSaved = (char *) psPH->pcBuf + psT->end ;
		char cSaved = *pSaved ;							// Save char before overwrite
		*pSaved = CHR_NUL ;								// terminate
		if ((psT->type == JSMN_PRIMITIVE) && (cvF != vfSXX) && pDst.pu8) {
			if (*pcBuf == CHR_n || *pcBuf == CHR_f) {
				pcBuf[0] = CHR_0 ;						// default 'null' & 'false' to 0
				pcBuf[1] = CHR_NUL ;
			} else if (*pcBuf == CHR_t) {
				pcBuf[0] = CHR_1 ;						// default 'true' to 1
				pcBuf[1] = CHR_NUL ;
			}
			if (cvParseValue(pcBuf, cvI, pDst) == pvFAILURE) {
				*pSaved = cSaved ;
				return erFAILURE ;
			}
			++NumOK ;
			++psPH->jtI ;
			pDst.pu8 += xIndex2Field(cvI) ;
		} else if (psT->type == JSMN_STRING && cvF == vfSXX && Hdlr) {
			int iRV = Hdlr(pcBuf) ;
			if (iRV < erSUCCESS) {
				*pSaved = cSaved ;
				return iRV ;
			}
			++NumOK ;
			++psPH->jtI ;
		} else {
			return erFAILURE ;
		}
		*pSaved = cSaved ;
	}
	return NumOK ;
}

int	xJsonParseList(const parse_list_t * psPlist, size_t szPList, const char * pcBuf, size_t szBuf, void * pvArg) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(psPlist) && szPList > 0) ;
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pcBuf) && szBuf > 0) ;
	parse_hdlr_t sPH = { 0 } ;
	int iRV = 0 ;
	iRV = xJsonParse(pcBuf, szBuf, &sPH.sParser, &sPH.psTList) ;
	if (iRV < 1 || sPH.psTList == NULL) {
		IF_P(debugRESULT, "jsmnX parse error (%d)\r\n", iRV) ;
		return erFAILURE ;
	}
	sPH.pcBuf	= pcBuf ;
	sPH.szBuf	= szBuf ;
	sPH.pvArg	= pvArg ;
	sPH.NumTok	= iRV ;
	for (sPH.plI = 0; sPH.plI < szPList; ++sPH.plI) {			// Outside parse_list_t loop
		sPH.pcKey = psPlist[sPH.plI].pToken ;
		sPH.szKey = strlen(sPH.pcKey) ;
		sPH.jtI	= 0 ;
		while(sPH.jtI < sPH.NumTok) {							// Inside jsmntok loop
			jsmntok_t * psT = &sPH.psTList[sPH.jtI] ;
			if ((sPH.szKey == (psT->end-psT->start)) &&
				(xstrncmp(sPH.pcKey, pcBuf+psT->start,sPH.szKey,1) == 1)) {
				IF_EXEC_1(debugLIST, xJsonPrintCurTok, &sPH) ;
				++sPH.jtI ;										// ensure start at next
				iRV = psPlist[sPH.plI].pHdlr(&sPH) ;
				if (iRV >= erSUCCESS) {
					sPH.NumOK += iRV ;
				}
			} else {
				++sPH.jtI ;										// skip this token
			}
		}
	}
	IF_EXEC_4(debugLIST && iRV >= erSUCCESS, xJsonPrintTokens, pcBuf, sPH.psTList, sPH.NumTok, 0) ;
	vRtosFree(sPH.psTList) ;
    return sPH.NumOK ? sPH.NumOK : iRV ;
}
