/*
 * Copyright 2014-21 Andre M. Maree/KSS Technologies (Pty) Ltd.
 */

/*
 * writerX.h
 */

#pragma once

#include	"x_complex_vars.h"
#include	"x_ubuf.h"
#include	"x_errors_events.h"

// ########################################## macros ##############################################


// ######################################## enumerations ###########################################

enum { jsonNULL= 0, jsonFALSE, jsonTRUE, jsonXXX, jsonSXX, jsonEDTZ, jsonARRAY, jsonOBJ } ;

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

typedef struct json_obj_t {
	struct json_obj_t *	parent ;
	struct json_obj_t *	child ;
    ubuf_t *		psBuf ;
    struct {
    	uint8_t		obj_nest : 4 ;                      // count OBJECT nesting level in this object
    	uint8_t		arr_nest : 4 ;						// count ARRAY nesting level in this object
    	uint8_t		val_count : 8 ;						// max 255 values per object
    	uint8_t		f_NoSep : 1 ;						// once off separator skip..
    	uint8_t		type ;
    } ;
} json_obj_t ;

// ####################################### global functions ########################################

int32_t	ecJsonSetDecimals(int32_t xNumber) ;
int32_t	ecJsonAddKeyValue(json_obj_t * pJson, const char * pKey, px_t pValue, uint8_t jForm, cv_idx_t cvI, size_t xArrSize) ;
int32_t	ecJsonCloseObject(json_obj_t * pJson) ;
int32_t	ecJsonCreateObject(json_obj_t * pJson, ubuf_t * psBuf) ;
