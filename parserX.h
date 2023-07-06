/*
 * parserX.h
 * Copyright 2014-22 Andre M. Maree/KSS Technologies (Pty) Ltd.
 */

#pragma once

#define JSMN_HEADER
#include "jsmn.h"
#include "database.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## macros ###############################################


// ######################################## enumerations ###########################################


// ############################################ structures #########################################

typedef struct {
	char * pcBuf;										// JSON source buffer
	size_t szBuf;										// source buffer size
	jsmn_parser sParser;								// jsmn control structure
	jsmntok_t *	psTList;								// jsmntok_t array allocated memory
	int NumTok;											// number of tokens parsed by jsmn
	int jtI;											// jsmntok_t index
	void * pvArg;
} parse_hdlr_t;

typedef struct {
	char * pToken;
	int (* pHdlr)(parse_hdlr_t *);
} ph_list_t;

// ####################################### global functions ########################################

void xJsonPrintToken(char * pcBuf, jsmntok_t * psT);
void xJsonPrintCurTok(parse_hdlr_t * psPH, const char * pLabel);

int	xJsonPrintTokens(char * pcBuf, jsmntok_t * pToken, size_t Count, int Depth);
int	xJsonParse(char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTokenList);

bool xJsonTokenIsKey(char * pBuf, jsmntok_t * pToken);

int	xJsonFindToken(char * pBuf, jsmntok_t * pTokenList, int NumTok, const char * pKey, bool Key);
#define xJsonFindValue(pBuf, pTL, numTok, pK) xJsonFindToken(pBuf, pTL, numTok, pK, false)
#define xJsonFindKey(pBuf, pTL, numTok, pK) xJsonFindToken(pBuf, pTL, numTok, pK, true)

int xJsonFindKeyValue(char * pBuf, jsmntok_t * psT, int NumTok, const char * pK, const char * pV);
int	xJsonParseKeyValue(char * pBuf, jsmntok_t * psT, int NumTok, const char * pK, px_t pX, cvi_e cvI);

#ifdef __cplusplus
}
#endif
