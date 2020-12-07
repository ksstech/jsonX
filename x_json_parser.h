/*
 * Copyright 2014-18 AM Maree/KSS Technologies (Pty) Ltd.
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
 * x_json_parser.h
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
	const 	char * 	pcBuf ;
	size_t			szBuf ;
	const 	char *	pcKey ;								// current key being evaluated
	size_t			szKey ;
	jsmn_parser		sParser ;
	jsmntok_t *		psTokenList ;
	int32_t			NumTok ;							// total number of tokens
	int32_t			NumOK ;								// number tokens parsed OK
	uint32_t		flag ;
	int32_t			i1 ;								// parse_list_t table index
	int32_t			i2 ;								// jsmntok_t table index
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
int32_t	xJsonParseKeyValue(const char * pBuf, jsmntok_t * pToken, int32_t NumTok, const char * pKey, void * pValue, varform_t VarForm) ;

int32_t	xJsonParseList(const parse_list_t * psPlist, size_t szPlist, const char * pcBuf, size_t szBuf) ;
