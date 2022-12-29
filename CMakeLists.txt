# JSONX using JSMN

set( srcs "jsmn.c" "parserX.c" "writerX.c" )
set( include_dirs "." )
set( priv_include_dirs )
set( requires
	buffers common database jsmn 
)
set( priv_requires 
	hal_esp32 
	main
	printfx 
	stringsX 
	syslog 
)

idf_component_register(
	SRCS ${srcs} 
	INCLUDE_DIRS ${include_dirs} 
	PRIV_INCLUDE_DIRS ${priv_include_dirs} 
	REQUIRES ${requires} 
	PRIV_REQUIRES ${priv_requires}
)
