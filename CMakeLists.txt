idf_component_register(
	SRCS "parserX.c" "writerX.c"
	INCLUDE_DIRS "."
	PRIV_INCLUDE_DIRS
	REQUIRES common jsmn statistics
	PRIV_REQUIRES printfx stringsX
)
