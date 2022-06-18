/*
 * Copyright 2014-21 Andre M. Maree/KSS Technologies (Pty) Ltd.
 */

/*
 * parserX.h
 */

#pragma once

#include	"jsmn.h"
#include	"database.h"

// ########################################## macros ###############################################


// ######################################## enumerations ###########################################


// ############################################ structures #########################################

typedef struct {
	const char *pcBuf ;									// JSON source buffer
	size_t		szBuf ;									// source buffer size
	const char *pcKey ;									// current key in parse_list_t
	size_t		szKey ;									// current key size
	jsmn_parser sParser ;								// jsmn control structure
	jsmntok_t *	psTList ;								// jsmntok_t array allocated memory
	int32_t		NumTok ;								// number of tokens parsed by jsmn
	int32_t		NumOK ;									// sum of values returned by handlers.
	int32_t		plI ;									// parse_list_t index
	int32_t		jtI ;									// jsmntok_t index
	void *		pvArg ;
} parse_hdlr_t ;

typedef struct {
	char * pToken ;
	int (* pHdlr)(parse_hdlr_t *) ;
} parse_list_t ;

// ####################################### global functions ########################################

void	xJsonPrintCurTok(parse_hdlr_t * psPH) ;

int	xJsonPrintTokens(const char * pcBuf, jsmntok_t * pToken, size_t Count, int Depth) ;
int	xJsonParse(const char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTokenList) ;

int	xJsonReadValue(const char * pBuf, jsmntok_t * pTokenList, double * pDouble) ;
int	xJsonCompareKey(const char * pKey, int TokLen, char * pTok) ;
int	xJsonFindKey(const char * pBuf, jsmntok_t * pTokenList, int NumTok, const char * pKey) ;

int	xJsonParseKeyValue(const char * pBuf, jsmntok_t * psT, int NumTok, const char * pKey, void * pValue, vf_e VarForm) ;

int xJsonParseArrayDB(parse_hdlr_t * psPH, px_t paDst[], int szArr, dbf_t paDBF[]) ;
int xJsonParseArray(parse_hdlr_t * psPH, px_t pDst, int(* Hdlr)(char *), int szArr, vf_e cvF, vs_e cvS) ;

int	xJsonParseList(const parse_list_t * psPlist, size_t szPlist, const char * pcBuf, size_t szBuf, void * pvArg) ;
