# JSONX
set( srcs "parserX.c" "writerX.c" )
set( include_dirs "." )
set( requires buffers common database jsmn statistics )
set( priv_requires hal_esp32 printfx stringsX syslog )
idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires} PRIV_REQUIRES ${priv_requires})
