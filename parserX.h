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
	const char * pcBuf;									// JSON source buffer
	size_t szBuf;										// source buffer size
	jsmn_parser sParser;								// jsmn control structure
	jsmntok_t *	psTList;								// jsmntok_t array allocated memory
	int NumTok;											// number of tokens parsed by jsmn
	int NumOK;											// sum of values returned by handlers.
	int jtI;											// jsmntok_t index
	void * pvArg;
} parse_hdlr_t;

typedef struct {
	char * pToken;
	int (* pHdlr)(parse_hdlr_t *);
} ph_list_t;

// ####################################### global functions ########################################

void xJsonPrintCurTok(parse_hdlr_t * psPH);

int	xJsonPrintTokens(const char * pcBuf, jsmntok_t * pToken, size_t Count, int Depth);
int	xJsonParse(const char * pBuf, size_t xLen, jsmn_parser * pParser, jsmntok_t * * ppTokenList);

bool xJsonTokenIsKey(const char * pBuf, jsmntok_t * pToken);

int	xJsonFindToken(const char * pBuf, jsmntok_t * pTokenList, int NumTok, const char * pKey, bool Key);
#define xJsonFindValue(pBuf, pTL, numTok, pK) xJsonFindToken(pBuf, pTL, numTok, pK, false)
#define xJsonFindKey(pBuf, pTL, numTok, pK) xJsonFindToken(pBuf, pTL, numTok, pK, true)

int xJsonFindKeyValue(const char * pBuf, jsmntok_t * psT, int NumTok, const char * pK, const char * pV);
int	xJsonParseKeyValue(const char * pBuf, jsmntok_t * psT, int NumTok, const char * pK, px_t pX, cvi_e cvI);
int xJsonParseArrayDB(parse_hdlr_t * psPH, px_t paDst[], int szArr, dbf_t paDBF[]);
int xJsonParseArray(parse_hdlr_t * psPH, px_t pDst, int(* Hdlr)(char *), int szArr, cvi_e cvI);
int	xJsonParsePayload(parse_hdlr_t * psPH, const ph_list_t * psPL, size_t szPL);
int	xJsonParseList(const ph_list_t * psPlist, size_t szPlist, const char * pcBuf, size_t szBuf, void * pvArg);

#ifdef __cplusplus
}
#endif
