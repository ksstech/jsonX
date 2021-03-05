idf_component_register(
	SRCS "parserX.c" "writerX.c"
	INCLUDE_DIRS "."
	PRIV_INCLUDE_DIRS
	REQUIRES common jsmn statistics
	PRIV_REQUIRES printfx stringsX
)
#add_compile_definitions( JSMN_PARENT_LINKS )
#add_compile_definitions( JSMN_STRICT )