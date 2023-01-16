cmake_minimum_required(VERSION 2.8.0)

aux_source_directory(. tinyui_src)

set (DIR_SRCS ${tinyui_src})

add_library( tinyui
	SHARED
	${DIR_SRCS})

target_link_libraries( tinyui
        pthread
        cdx_base
        xml2
        minigui_ths ts png jpeg z m freetype bz2)

