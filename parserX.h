// parserX.h

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
	const char * pcBuf;										// JSON source buffer
	size_t szBuf;										// source buffer size
	jsmn_parser sParser;								// control structure
	jsmntok_t *	psT0;									// jsmntok_t array allocated memory
	jsmntok_t *	psTx;									// Current token being processed
	int NumTok;											// number of tokens parsed
	int CurTok;											// index of current token being processed
	void * pvArg;
} parse_hdlr_t;

typedef struct {
	char * pToken;
	int (* pHdlr)(parse_hdlr_t *);
} ph_list_t;

typedef struct ph_entry_t {
	const char * pcKey;
	px_t pxVar;
	cvi_e cvI;
} ph_entry_t;

typedef struct ph_entries_t {
	u8_t Count;
	ph_entry_t Entry[];
} ph_entries_t;

// ####################################### global functions ########################################

int xJsonParse(parse_hdlr_t * psPH);
int xJsonFindToken(parse_hdlr_t * psPH, const char * pKey, int Key);
int xJsonFindKeyValue(parse_hdlr_t * psPH, const char * pK, const char * pV);
int xJsonFindKeyValue(parse_hdlr_t * psPH, const char * pK, const char * pV);
int xJsonParseEntry(parse_hdlr_t * psPH, ph_entry_t * psEntry);

/**
 * @brief
 */
void xJsonPrintCurTok(report_t * psR, parse_hdlr_t * psPH, const char * pLabel);

#ifdef __cplusplus
}
#endif
