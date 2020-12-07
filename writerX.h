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
 * writerX.h
 */

#pragma once

#include	"x_definitions.h"							// brings mySTRINGIFY
#include	"x_ubuf.h"
#include	"x_errors_events.h"
#include	"x_struct_union.h"

// ########################################## macros ##############################################

#define	buildJSON_FLOAT_FORMAT			"%." mySTRINGIFY(buildJSON_FLOAT_DECIMALS) "f"

// ######################################## enumerations ###########################################

enum { jsonNULL= 0, jsonFALSE, jsonTRUE, jsonNUMBER, jsonSTRING, jsonDATETIME, jsonARRAY, jsonOBJECT, jsonNUM, jsonMAX } ;

enum {
    jsonFORM_U08, jsonFORM_I08, jsonFORM_F08, jsonFORM_X08,
    jsonFORM_U16, jsonFORM_I16, jsonFORM_F16, jsonFORM_X16,
    jsonFORM_U32, jsonFORM_I32, jsonFORM_F32, jsonFORM_X32,
    jsonFORM_U64, jsonFORM_I64, jsonFORM_F64, jsonFORM_X64,
	jsonFORM_DT_UTC,
	jsonFORM_DT_TZ,
	jsonFORM_DT_ELAP,
	jsonFORM_DT_ALT,
	jsonFORM_STRING,
	jsonFORM_BOOLEAN,
    jsonFORM_NAN,
} ;

enum {
	erJSON_CREATE			= -3110,							// start 10 down from Appl error codes
	erJSON_NEST_LEV,
    erJSON_BUF_FULL,
	erJSON_VAL_TYPE,
    erJSON_NUM_TYPE,
	erJSON_TYPE,
	erJSON_FORMAT,
	erJSON_ARRAY,
    erJSON_UNDEF,
}  ;

// ############################################ structures #########################################

typedef struct json_obj_s json_obj_t ;

struct json_obj_s {
	json_obj_t *	parent ;
	json_obj_t *	child ;
    ubuf_t *		psBuf ;
    struct {
    	uint8_t		obj_nest : 4 ;                      // count OBJECT nesting level in this object
    	uint8_t		arr_nest : 4 ;						// count ARRAY nesting level in this object
    	uint8_t		val_count : 8 ;						// max 255 values per object
    	uint8_t		f_NoSep : 1 ;						// once off separator skip..
    	uint8_t		type ;
    } ;
} ;

// ####################################### global functions ########################################

int32_t	xJsonMapNumType(varsize_t vs, varform_t vf) ;
int32_t	ecJsonSetDecimals(int32_t xNumber) ;
int32_t	ecJsonAddKeyValue(json_obj_t * pJson, const char * pKey, p32_t pValue, uint8_t eType, uint8_t eFormType, size_t xArrSize) ;
int32_t	ecJsonCloseObject(json_obj_t * pJson) ;
int32_t	ecJsonCreateObject(json_obj_t * pJson, ubuf_t * psBuf) ;
