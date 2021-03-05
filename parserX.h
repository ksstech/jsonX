/*
 * Copyright 2014-21 Andre M. Maree/KSS Technologies (Pty) Ltd.
 */

/*
 * parserX.h
 */

#pragma once

#include	"jsmn.h"

#include	"x_definitions.h"
#include	"x_buffers.h"
#include	"x_complex_vars.h"

// ########################################## macros ###############################################


// ######################################## enumerations ###########################################


// ############################################ structures #########################################

typedef struct {
	const char *pcBuf ;
	size_t		szBuf ;
	const char *pcKey ;									// current key being evaluated
	size_t		szKey ;
	jsmn_parser	sParser ;
	jsmntok_t *	psTokenList ;
	int32_t		NumTok ;								// total number of tokens
	int32_t		NumOK ;									// number tokens parsed OK
	uint32_t	flag ;
	int32_t		plI ;									// parse_list_t index
	int32_t		jtI ;									// jsmntok_t index
	void *		pvArg ;
} parse_hdlr_t ;

typedef struct {
	char * pToken ;
	int (* pHdlr)(parse_hdlr_t *) ;
} parse_list_t ;

// ####################################### global functions ########################################

void	xJsonPrintToken(const char * pcBuf, jsmntok_t * pT) ;

int32_t	xJsonPrintTokens(const char * pcBuf, jsmntok_t * pToken, size_t Count, int32_t Depth) ;
int32_t	xJsonParse(const char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTokenList) ;

int32_t	xJsonReadValue(const char * pBuf, jsmntok_t * pTokenList, double * pDouble) ;
int32_t	xJsonCompareKey(const char * pKey, int32_t TokLen, char * pTok) ;
int32_t	xJsonFindKey(const char * pBuf, jsmntok_t * pTokenList, int32_t NumTok, const char * pKey) ;

int32_t	xJsonParseKeyValue(const char * pBuf, jsmntok_t * psT, int32_t NumTok, const char * pKey, void * pValue, varform_t VarForm) ;

int32_t xJsonParseArray(parse_hdlr_t * psPH, p32_t pDst, int32_t(* Hdlr)(char *), int32_t Count, varform_t cvF, varsize_t cvS) ;

int32_t	xJsonParseList(const parse_list_t * psPlist, size_t szPlist, const char * pcBuf, size_t szBuf, void * pvArg) ;
