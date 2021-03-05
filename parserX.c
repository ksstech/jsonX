/*
 * parserX.c
 * Copyright 2014-21 Andre M. Maree/KSS Technologies (Pty) Ltd.
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 *
 */

#include	"jsmn.h"

#include	"parserX.h"
#include	"printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include	"syslog.h"
#include	"x_errors_events.h"
#include	"x_string_general.h"
#include	"x_string_to_values.h"
#include	"x_complex_vars.h"

#include	"hal_config.h"

#include	<string.h>
#include	<ctype.h>

// ############################### BUILD: debug configuration options ##############################

#define	debugFLAG					0xC000

#define	debugFINDKEY				(debugFLAG & 0x0001)
#define	debugHDLR					(debugFLAG & 0x0002)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ####################################### Global Functions ########################################

void	xJsonPrintIndent(int Depth, int Sep, int CR0, int CR1) {
	if (CR0)							printfx("\n") ;
	for (int x = 0; x < Depth; ++x)		printfx("  ") ;
	if (Sep)							printfx(" %c", Sep) ;
	if (CR0)							printfx("(s=%d)", CR0) ;
	if (CR1)							printfx("\n") ;
}

void	 xJsonPrintToken(const char * pcBuf, jsmntok_t * pT) {
	printfx("t=%d s=%d b=%d e=%d '%.*s'\n", pT->type, pT->size, pT->start, pT->end, pT->end - pT->start, pcBuf+pT->start) ;
}

int32_t	 xJsonPrintTokens(const char * pcBuf, jsmntok_t * pToken, size_t Count, int Depth) {
	if (Count == 0) {
		return erSUCCESS ;
	}
	if (pToken->type == JSMN_PRIMITIVE || pToken->type == JSMN_STRING) {
		printfx("%d='%.*s'", pToken->type, pToken->end - pToken->start, pcBuf+pToken->start) ;
		return 1 ;

	} else if (pToken->type == JSMN_OBJECT) {
		xJsonPrintIndent(Depth, CHR_L_CURLY, pToken->size, 1) ;
		int j = 0 ;
		for (int i = 0; i < pToken->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0) ;
			j += xJsonPrintTokens(pcBuf, pToken+j+1, Count-j, Depth+1) ;
			printfx(" : ") ;
			j += xJsonPrintTokens(pcBuf, pToken+j+1, Count-j, Depth+1) ;
			printfx("\n") ;
		}
		xJsonPrintIndent(Depth, CHR_R_CURLY, 0, 0) ;
		return j + 1 ;

	} else if (pToken->type == JSMN_ARRAY) {
		xJsonPrintIndent(Depth, CHR_L_SQUARE, pToken->size, 1) ;
		int j = 0 ;
		for (int i = 0; i < pToken->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0) ;
			j += xJsonPrintTokens(pcBuf, pToken+j+1, Count-j, Depth+1) ;
			printfx("\n") ;
		}
		xJsonPrintIndent(Depth, CHR_R_SQUARE, 0, 0) ;
		return j + 1 ;
	}
	return 0 ;
}

/*
 * xJsonParse()
 */
int32_t	xJsonParse(const char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTokenList) {
	int32_t iRV1 = 0, iRV2 = 0 ;
	jsmn_init(pParser) ;
	*ppTokenList = NULL ;				// default allocation pointer to NULL
	iRV1 = jsmn_parse(pParser, (const char *) pBuf, xLen, NULL, 0) ;	// count tokens
	if (iRV1 < 1) {
		goto exit ;
	}

	*ppTokenList = (jsmntok_t *) malloc(iRV1 * sizeof(jsmntok_t)) ;	// alloc buffer
	jsmn_init(pParser) ;								// Init & do actual parse...
	iRV2 = jsmn_parse(pParser, (const char *) pBuf, xLen, *ppTokenList, iRV1) ;
	IF_EXEC_4(debugTRACK, xJsonPrintTokens, pBuf, *ppTokenList, iRV1, 0) ;
exit:
	IF_SL_INFO(debugRESULT, "Result=%d/%d '%s'", iRV1, iRV2, iRV1<0 || iRV1!=iRV2 ? "ERROR" : "Parsed") ;
	return iRV1 < 1 ? iRV1 : iRV2 ;
}

/**
 * xJsonReadValue() -
 * @param pBuf
 * @param pToken
 * @param pDouble
 * @return			erSUCCESS or erFAILURE
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
	IF_SL_INFO(debugFINDKEY, "\r\n%s: Key='%s'", __FUNCTION__, pKey) ;
	int32_t	KeyLen = xstrlen(pKey) ;
	for (int32_t CurTok = 0; CurTok < NumTok; CurTok++, pToken++) {
		if (KeyLen != (pToken->end - pToken->start)) {
			continue ;							// not same length, skip
		}
		IF_SL_INFO(debugFINDKEY, "[Tok='%.*s'] ", pToken->end - pToken->start, pBuf + pToken->start) ;
		if (xJsonCompareKey(pKey, KeyLen, (char *) pBuf + pToken->start) == erFAILURE) {
			continue ;							// not matching, skip
		}
		return CurTok ;							// all OK, return token number
	}
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
 * Parse JSON array for strings or primitives
 * @param	psPH	Pointer to pre-initialised parser handler structure
 * @param	pDst	Pointer to storage array for primitive element (CHECK SIZE !!!)
 * @param	Hdlr	Handler function to process string elements
 * @param	Count	Maximum number of elements to parse
 * @param	cvF		Format of primitive elements
 * @param	cvS		Size of primitive element values
 * @return
 */
int32_t xJsonParseArray(parse_hdlr_t * psPH, p32_t pDst, int32_t(* Hdlr)(char *), int32_t Count, varform_t cvF, varsize_t cvS) {
	int32_t szArr = psPH->psTokenList[++psPH->jtI].size ;	// Also skip over ARRAY token
	IF_myASSERT(debugPARAM, szArr >= Count) ;
	if (Count == 0) {
		Count = szArr ;
	}
	int32_t iRV = erFAILURE ;
	jsmntok_t * psT = &psPH->psTokenList[psPH->jtI] ;
	for (int32_t i = 0; i < Count; ++psT, ++psPH->jtI, ++i) {
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
			pcBuf = pcStringParseValue(pcBuf, pDst, cvF, cvS, NULL) ;
			pDst.pu8 += cvS == vs64B ? sizeof(uint64_t) : cvS == vs32B ? sizeof(uint32_t) :
						cvS == vs16B ? sizeof(uint16_t) : sizeof(uint8_t) ;
			if (pcBuf == pcFAILURE)
				iRV = erFAILURE ;
		} else if ((psT->type == JSMN_STRING) && (cvF == vfSXX) && Hdlr) {
			iRV = Hdlr(pcBuf) ;
		} else {
			myASSERT(0) ;
		}
		*pSaved = cSaved ;
		if (iRV == erFAILURE) {
			return erFAILURE ;
		}
	}
	return szArr ;
}

int32_t	xJsonParseList(const parse_list_t * psPlist, size_t szPlist, const char * pcBuf, size_t szBuf, void * pvArg) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(psPlist) && szPlist > 0) ;
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pcBuf) && szBuf > 0) ;
	parse_hdlr_t sPH = { 0 } ;
	sPH.pvArg	= pvArg ;
	int32_t iRV1 = 0, iRV2 = 0 ;
	iRV1 = xJsonParse(pcBuf, szBuf, &sPH.sParser, &sPH.psTokenList) ;
	if (iRV1 < 1 || sPH.psTokenList == NULL) {
		goto no_free ;
	}
	for (sPH.plI = 0, sPH.NumTok = iRV1; sPH.plI < szPlist; ++sPH.plI) {
		sPH.pcBuf = pcBuf ;
		sPH.szBuf = szBuf ;
		sPH.pcKey = psPlist[sPH.plI].pToken ;
		sPH.szKey = xstrlen(sPH.pcKey) ;
		for (sPH.jtI = 0; sPH.jtI < sPH.NumTok; ++sPH.jtI) {
			jsmntok_t * psToken = &sPH.psTokenList[sPH.jtI] ;
			if (sPH.szKey == (psToken->end - psToken->start) &&
				xstrncmp(sPH.pcKey, pcBuf+psToken->start, sPH.szKey, 1)) {
//				TRACK("szKey=%d  '%.*s'", sPH.szKey, psToken->end - psToken->start, pcBuf+psToken->start) ;
				iRV2 = psPlist[sPH.plI].pHdlr(&sPH) ;
				if (iRV2 >= erSUCCESS) {
					++sPH.NumOK ;
				} else {
					SL_WARN("iRV=%d  Key='%s'  Tok#=%d  Val='%.*s'", iRV2, sPH.pcKey, sPH.jtI, psToken->end - psToken->start, pcBuf + psToken->start) ;
				}
			}
		}
	}
	IF_EXEC_4(debugHDLR && iRV2 >= erSUCCESS, xJsonPrintTokens, pcBuf, sPH.psTokenList, sPH.NumTok, 0) ;
	free(sPH.psTokenList) ;
no_free:
	IF_TRACK(debugRESULT && iRV1 < 1, "ERROR=%d", iRV1) ;
    return sPH.NumOK ? sPH.NumOK : iRV2 ;
}
