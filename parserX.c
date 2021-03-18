/*
 * parserX.c
 * Copyright 2014-21 Andre M. Maree/KSS Technologies (Pty) Ltd.
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 *
 */

#include	"parserX.h"
#include	"x_string_general.h"
#include	"x_string_to_values.h"
#include	"x_errors_events.h"
#include	"printfx.h"									// x_definitions stdarg stdint stdio

#include	"hal_config.h"

#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

// ############################### BUILD: debug configuration options ##############################

#define	debugFLAG					0xF000

#define	debugFINDKEY				(debugFLAG & 0x0001)
#define	debugHDLR					(debugFLAG & 0x0002)
#define	debugPARSE					(debugFLAG & 0x0004)
#define	debugARRAY					(debugFLAG & 0x0008)

#define	debugLIST					(debugFLAG & 0x0010)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ####################################### Global Functions ########################################

void	xJsonPrintCurTok(parse_hdlr_t * psPH) {
	jsmntok_t * psT = &psPH->psTList[psPH->jtI] ;
	printfx("#%d/%d  t=%d  s=%d  b=%d  e=%d  '%.*s'\n", psPH->jtI, psPH->NumTok, psT->type,
		psT->size, psT->start, psT->end, psT->end - psT->start, psPH->pcBuf + psT->start) ;
}

void	xJsonPrintIndent(int Depth, int Sep, int CR0, int CR1) {
	if (CR0)							printfx("\n") ;
	for (int x = 0; x < Depth; ++x)		printfx("  ") ;
	if (Sep)							printfx(" %c", Sep) ;
	if (CR0)							printfx("(s=%d)", CR0) ;
	if (CR1)							printfx("\n") ;
}

/**
 * Iterates through the token list and prints all tokens, type, size and actual value
 * @param pcBuf
 * @param psT
 * @param Count
 * @param Depth
 * @return
 */
int32_t	 xJsonPrintTokens(const char * pcBuf, jsmntok_t * psT, size_t Count, int Depth) {
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
			printfx("\n") ;
		}
		xJsonPrintIndent(Depth, CHR_R_CURLY, 0, 0) ;
		return j + 1 ;

	} else if (psT->type == JSMN_ARRAY) {
		xJsonPrintIndent(Depth, CHR_L_SQUARE, psT->size, 1) ;
		int j = 0 ;
		for (int i = 0; i < psT->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0) ;
			j += xJsonPrintTokens(pcBuf, psT+j+1, Count-j, Depth+1) ;
			printfx("\n") ;
		}
		xJsonPrintIndent(Depth, CHR_R_SQUARE, 0, 0) ;
		return j + 1 ;
	}
	printfx("\n") ;
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
int32_t	xJsonParse(const char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTokenList) {
	int32_t iRV1 = 0, iRV2 = 0 ;
	jsmn_init(pParser) ;
	*ppTokenList = NULL ;				// default allocation pointer to NULL
	iRV1 = jsmn_parse(pParser, (const char *) pBuf, xLen, NULL, 0) ;	// count tokens
	if (iRV1 <= 0) {
		IF_PRINT(debugPARSE, "Failed (%d)\n", iRV1) ;
		return iRV1 ;
	}

	*ppTokenList = (jsmntok_t *) malloc(iRV1 * sizeof(jsmntok_t)) ;		// alloc buffer
	jsmn_init(pParser) ;								// Init & do actual parse...
	iRV2 = jsmn_parse(pParser, (const char *) pBuf, xLen, *ppTokenList, iRV1) ;
	IF_EXEC_4(debugPARSE, xJsonPrintTokens, pBuf, *ppTokenList, iRV1, 0) ;
	if (iRV1 != iRV2) {
		free(*ppTokenList) ;
		*ppTokenList = NULL ;
		IF_PRINT(debugPARSE, "Failed (%d != %d)\n", iRV1, iRV2) ;
		return iRV1 ;
	}
	IF_PRINT(debugPARSE, "Passed (%d) parsed OK\n", iRV2) ;
	return iRV2 ;
}

/**
 * xJsonReadValue() -
 * @param	pBuf
 * @param	pToken
 * @param	pDouble
 * @return	erSUCCESS or erFAILURE
 */
int32_t	xJsonReadValue(const char * pBuf, jsmntok_t * pToken, double * pDouble) {
	char * 	pSrc = (char *) (pBuf + pToken->start) ;
	int32_t	Sign ;
	if (pcStringParseF64(pSrc, pDouble, &Sign, " ,") == pcFAILURE) {
		return erFAILURE ;
	}
	return erSUCCESS ;
}

int32_t	xJsonCompareKey(const char * pKey, int32_t TokLen, char * pTok) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(pKey) && halCONFIG_inMEM(pTok) && TokLen > 0) ;
	while (*pKey && *pTok && TokLen) {
		if (toupper((int)*pKey) != toupper((int)*pTok)) {
		    return erFAILURE ;
		}
		++pKey ;
		++pTok ;
		--TokLen ;
	}
	return (*pKey == CHR_NUL && TokLen == 0) ? erSUCCESS : erFAILURE ;
}

int32_t	xJsonFindKey(const char * pBuf, jsmntok_t * pToken, int32_t NumTok, const char * pKey) {
	IF_PRINT(debugFINDKEY, "\r\n%s: Key='%s'\n", __FUNCTION__, pKey) ;
	int32_t	KeyLen = xstrlen(pKey) ;
	for (int32_t CurTok = 0; CurTok < NumTok; CurTok++, pToken++) {
		if (KeyLen != (pToken->end - pToken->start)) {
			continue ;							// not same length, skip
		}
		IF_PRINT(debugFINDKEY, "[Tok='%.*s'] ", pToken->end - pToken->start, pBuf + pToken->start) ;
		if (xJsonCompareKey(pKey, KeyLen, (char *) pBuf + pToken->start) == erFAILURE) {
			continue ;							// not matching, skip
		}
		IF_PRINT(debugFINDKEY, "\n") ;
		return CurTok ;							// all OK, return token number
	}
	IF_PRINT(debugFINDKEY, "\n") ;
	return erFAILURE ;
}

/**
 * xJsonParseKeyValue() -
 * @param pBuf
 * @param pToken
 * @param NumTok
 * @param pKey
 * @param pValue
 * @param VarForm
 * @return			erSUCCESS or erFAILURE
 */
int32_t	xJsonParseKeyValue(const char * pBuf, jsmntok_t * pToken, int32_t NumTok, const char * pKey, void * pValue, varform_t VarForm) {
	int32_t	xLen ;
	char * pDst, * pSrc ;
	double	f64Value ;
	int32_t iRV = xJsonFindKey(pBuf, pToken, NumTok, pKey) ;
	if (iRV == erFAILURE) {
		return iRV ;
	}
	pToken += iRV + 1 ;									// step to token after matching token..
	if (VarForm == vfSXX) {
		IF_myASSERT(debugPARAM, pToken->type == JSMN_STRING) ;
		pDst = (char *) pValue ;
		pSrc = (char *) pBuf + pToken->start ;
		xLen = pToken->end - pToken->start ;			// calculate length
		strncpy(pDst, pSrc, xLen) ;
		*(pDst + xLen) = CHR_NUL ;						// strncpy with exact size, no termination..
		iRV = erSUCCESS ;
	} else {
		IF_myASSERT(debugPARAM, pToken->type == JSMN_PRIMITIVE) ;
		iRV = xJsonReadValue(pBuf, pToken, &f64Value) ;
		if (iRV == erSUCCESS) {
			switch(VarForm) {
			case vfUXX:	*((uint32_t *) pValue)	= (uint32_t) f64Value ;			break ;
			case vfIXX:	*((int32_t *) pValue)	= (int32_t) f64Value ;			break ;
			case vfFXX:	*((float *) pValue)		= (float) f64Value ;			break ;
			case vfSXX:															break ;
			}
		}
	}
	return iRV ;
}

/**
 * Parse JSON array for strings or primitives, store directly into empty db_t structure record
 * @brief	EXPECT jtI to be the token# of the array to be parsed.
 * @param	psPH	Pointer to pre-initialised parser handler structure
 * @param	Count	Maximum number of elements to parse
 * @param	paDst	Pointer to array of pointers for each element
 * @param	cvF		pointer to array of dbf_t structures defining each db_t record
 * @return	Number of elements parsed or erFAILURE
 * 			if successful, leaves jtI set to next token to parse
 * 			if failed, jti left at the failing element
 */
int32_t xJsonParseArrayDB(parse_hdlr_t * psPH, int32_t szArr, px_t paDst[], dbf_t paDBF[]) {
	IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH) ;
	if (psPH->psTList[psPH->jtI].size != szArr) {
		IF_PRINT(debugARRAY, "Invalid Array size (%u) or count(%u)\n", psPH->psTList[psPH->jtI].size, szArr) ;
		return erFAILURE ;
	}
	int32_t NumOK = 0 ;
	jsmntok_t * psT = &psPH->psTList[++psPH->jtI] ;		// step into ARRAY to 1st ELEMENT
	for (int32_t i = 0; i < szArr; ++psT, ++i) {
		varform_t cvF = xCV_Index2Form(paDBF[i].cvI) ;
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
			if (pcStringParseValue(pcBuf, paDst[i], cvF, xCV_Index2Size(paDBF[i].cvI), NULL) == pcFAILURE) {
				*pSaved = cSaved ;
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
int32_t xJsonParseArray(parse_hdlr_t * psPH, px_t pDst, int32_t(* Hdlr)(char *),
						int32_t szArr, varform_t cvF, varsize_t cvS) {
	IF_EXEC_1(debugARRAY, xJsonPrintCurTok, psPH) ;
	if (szArr < 1 || psPH->psTList[psPH->jtI].size != szArr) {
		IF_PRINT(debugARRAY, "Invalid Array size (%u) or count(%u)\n", psPH->psTList[psPH->jtI].size, szArr) ;
		return erFAILURE ;
	}
	int32_t NumOK = 0 ;
	jsmntok_t * psT = &psPH->psTList[++psPH->jtI] ;		// step to first ARRAY ELEMENT
	for (int32_t i = 0; i < szArr; ++psT, ++i) {
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
			if (pcStringParseValue(pcBuf, pDst, cvF, cvS, NULL) == pvFAILURE) {
				*pSaved = cSaved ;
				return erFAILURE ;
			}
			++NumOK ;
			++psPH->jtI ;
			pDst.pu8 += xCV_Size2Field(cvS) ;
		} else if (psT->type == JSMN_STRING && cvF == vfSXX && Hdlr) {
			int32_t iRV = Hdlr(pcBuf) ;
			if (iRV < erSUCCESS) {
				*pSaved = cSaved ;
				return erFAILURE ;
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

int32_t	xJsonParseList(const parse_list_t * psPlist, size_t szPList, const char * pcBuf, size_t szBuf, void * pvArg) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(psPlist) && szPList > 0) ;
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pcBuf) && szBuf > 0) ;
	parse_hdlr_t sPH = { 0 } ;
	int32_t iRV = 0 ;
	iRV = xJsonParse(pcBuf, szBuf, &sPH.sParser, &sPH.psTList) ;
	if (iRV < 1 || sPH.psTList == NULL) {
		IF_PRINT(debugRESULT, "jsmnX parse error (%d)\n", iRV) ;
		return erFAILURE ;
	}
	sPH.pcBuf	= pcBuf ;
	sPH.szBuf	= szBuf ;
	sPH.pvArg	= pvArg ;
	sPH.NumTok	= iRV ;
	for (sPH.plI = 0; sPH.plI < szPList; ++sPH.plI) {			// Outside parse_list_t loop
		sPH.pcKey = psPlist[sPH.plI].pToken ;
		sPH.szKey = xstrlen(sPH.pcKey) ;
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
	IF_EXEC_4(debugHDLR && iRV >= erSUCCESS, xJsonPrintTokens, pcBuf, sPH.psTList, sPH.NumTok, 0) ;
	free(sPH.psTList) ;
    return sPH.NumOK ? sPH.NumOK : iRV ;
}
