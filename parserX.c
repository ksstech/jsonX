/*
 * Copyright 2014-20 AM Maree/KSS Technologies (Pty) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * parserX.c
 *
 * References:
 * 	http://www.json.org/
 * 	http://json-schema.org/
 *
 */

//#define 	JSMN_PARENT_LINKS
//#define		JSMN_STRICT

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
	if (CR0)							PRINT("\n") ;
	for (int x = 0; x < Depth; ++x)		PRINT("  ") ;
	if (Sep)							PRINT(" %c", Sep) ;
	if (CR0)							PRINT("(s=%d)", CR0) ;
	if (CR1)							PRINT("\n") ;
}

void	 xJsonPrintToken(const char * pcBuf, jsmntok_t * pT) {
	printfx("t=%d s=%d b=%d e=%d '%.*s'\n", pT->type, pT->size, pT->start, pT->end, pT->end - pT->start, pcBuf+pT->start) ;
}

int32_t	 xJsonPrintTokens(const char * pcBuf, jsmntok_t * pToken, size_t Count, int Depth) {
	if (Count == 0)
		return erSUCCESS ;

	if (pToken->type == JSMN_PRIMITIVE || pToken->type == JSMN_STRING) {
		PRINT("%d='%.*s'", pToken->type, pToken->end - pToken->start, pcBuf+pToken->start) ;
		return 1 ;

	} else if (pToken->type == JSMN_OBJECT) {
		xJsonPrintIndent(Depth, CHR_L_CURLY, pToken->size, 1) ;
		int j = 0 ;
		for (int i = 0; i < pToken->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0) ;
			j += xJsonPrintTokens(pcBuf, pToken+j+1, Count-j, Depth+1) ;
			PRINT(" : ") ;
			j += xJsonPrintTokens(pcBuf, pToken+j+1, Count-j, Depth+1) ;
			PRINT("\n") ;
		}
		xJsonPrintIndent(Depth, CHR_R_CURLY, 0, 0) ;
		return j + 1 ;

	} else if (pToken->type == JSMN_ARRAY) {
		xJsonPrintIndent(Depth, CHR_L_SQUARE, pToken->size, 1) ;
		int j = 0 ;
		for (int i = 0; i < pToken->size; ++i) {
			xJsonPrintIndent(Depth+2, 0, 0, 0) ;
			j += xJsonPrintTokens(pcBuf, pToken+j+1, Count-j, Depth+1) ;
			PRINT("\n") ;
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
	if (iRV1 < 1)					goto exit ;

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

int32_t	xJsonParseList(const parse_list_t * psPlist, size_t szPlist, const char * pcBuf, size_t szBuf) {
	IF_myASSERT(debugPARAM, halCONFIG_inMEM(psPlist) && szPlist > 0) ;
	IF_myASSERT(debugPARAM, halCONFIG_inSRAM(pcBuf) && szBuf > 0) ;
	parse_hdlr_t sPH = { 0 } ;
	int32_t iRV1 = 0, iRV2 = 0 ;
	iRV1 = xJsonParse(pcBuf, szBuf, &sPH.sParser, &sPH.psTokenList) ;
	if (iRV1 < 1 || sPH.psTokenList == NULL)
		goto no_free ;

	for (sPH.i1 = 0, sPH.NumTok = iRV1; sPH.i1 < szPlist; ++sPH.i1) {
		sPH.pcBuf = pcBuf ;
		sPH.szBuf = szBuf ;
		sPH.pcKey = psPlist[sPH.i1].pToken ;
		sPH.szKey = xstrlen(sPH.pcKey) ;
		for (sPH.i2 = 0; sPH.i2 < sPH.NumTok; ++sPH.i2) {
			jsmntok_t * psToken = &sPH.psTokenList[sPH.i2] ;
			if (sPH.szKey == (psToken->end - psToken->start) &&
				xstrncmp(sPH.pcKey, pcBuf+psToken->start, sPH.szKey, true)) {
//				TRACK("szKey=%d  '%.*s'", sPH.szKey, psToken->end - psToken->start, pcBuf+psToken->start) ;
				iRV2 = psPlist[sPH.i1].pHdlr(&sPH) ;
				if (iRV2 >= erSUCCESS)
					++sPH.NumOK ;
				else
					SL_WARN("iRV=%d  Key='%s'  Tok#=%d  Val='%.*s'", iRV2, sPH.pcKey, sPH.i2, psToken->end - psToken->start, pcBuf + psToken->start) ;
			}
		}
	}
	IF_EXEC_4(debugHDLR && iRV2 >= erSUCCESS, xJsonPrintTokens, pcBuf, sPH.psTokenList, sPH.NumTok, 0) ;
	free(sPH.psTokenList) ;
no_free:
	IF_TRACK(debugRESULT && iRV1 < 1, "ERROR=%d", iRV1) ;
    return sPH.NumOK ? sPH.NumOK : iRV2 ;
}
