idf_component_register(
	SRCS "x_json_parser.c" "x_json_writer.c"
	INCLUDE_DIRS "."
	PRIV_INCLUDE_DIRS
	REQUIRES common jsmn statistics
	PRIV_REQUIRES printfx stringsX
)
