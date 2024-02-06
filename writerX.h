/*
 * writerX.h - Copyright (c) 2014-24 Andre M. Maree/KSS Technologies (Pty) Ltd.
 */

#pragma once

#include "complex_vars.h"
#include "x_ubuf.h"
#include "x_errors_events.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## macros ##############################################

#define	jsonHAS_TIMESTAMP			0

// ######################################## enumerations ###########################################

typedef enum {
	jsonNULL= 0,
	jsonFALSE,
	jsonTRUE,
	jsonXXX,
	jsonSXX,
	#if	(jsonHAS_TIMESTAMP == 1)
	jsonEDTZ,
	#endif
	jsonARRAY,
	jsonOBJ
} jform_t;

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
} ;

enum { jsonTYPE_NULL, jsonTYPE_ARRAY };

// ############################################ structures #########################################

typedef struct json_obj_t {
	struct json_obj_t *	parent;
	struct json_obj_t *	child;
    ubuf_t * psBuf;
    struct {
    	u8_t obj_nest:4;			// count OBJECT nesting level in this object
    	u8_t arr_nest:4;			// count ARRAY nesting level in this object
    	u8_t val_count:8;			// max 255 values per object
    	u8_t f_NoSep:1;				// once off separator skip..
    	u8_t type;
    };
} json_obj_t;

// ####################################### global functions ########################################

int	ecJsonSetDecimals(int xNumber);
int	ecJsonAddKeyValue(json_obj_t * pJson, const char * pKey, px_t pValue, jform_t jForm, cvi_e cvI, size_t xArrSize);
int	ecJsonCloseObject(json_obj_t * pJson);
int	ecJsonCreateObject(json_obj_t * pJson, ubuf_t * psBuf);

#ifdef __cplusplus
}
#endif
