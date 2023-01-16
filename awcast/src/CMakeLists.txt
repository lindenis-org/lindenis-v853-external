cmake_minimum_required(VERSION 2.8.0)

project(awcast)

# 告知当前使用的是交叉编译方式，必须配置
SET(CMAKE_SYSTEM_NAME Linux)

set (CMAKE_CXX_STANDARD 11)

# tina config
SET(TINA_DIR "$ENV{TINA_BUILD_TOP}")
#${TINA_BUILD_TOP}/out/${TARGET_BOARD}

SET(STAGING_DIR "${TINA_DIR}/out/$ENV{TARGET_BOARD}/staging_dir/")

SET(TINA_OUT_TARGET "${TINA_DIR}/out/$ENV{TARGET_BOARD}/staging_dir/target")

# toolchain config
SET(TOOLCHAIN_DIR "${TINA_DIR}/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-glibc/toolchain/bin")

set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/arm-openwrt-linux-gnueabi-g++")
set(CMAKE_C_COMPILER   "${TOOLCHAIN_DIR}/arm-openwrt-linux-gnueabi-gcc")

# 指定交叉编译环境安装目录...
SET(CMAKE_FIND_ROOT_PATH ${TINA_OUT_TARGET})

# 从来不在指定目录下查找工具程序
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

#只在指定目录下查找库文件
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

# 只在指定目录下查找头文件
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
#SET(OPENSSL_ROOT_DIR "${TINA_OUT_TARGET}/usr/include")
#SET(OPENSSL_ROOT_DIR "${TINA_DIR}/out/dolphin-p1/compile_dir/target/openssl-1.1.0j")
#SET(OPENSSL_INCLUDE_DIR "${TINA_OUT_TARGET}/usr/include")
#SET(OPENSSL_LIBRARIES "${TINA_OUT_TARGET}/usr/lib")

#set(CMAKE_C_FLAGS "-DHAVE_CONFIG_H")
#set(CMAKE_CXX_FLAGS "-DHAVE_CONFIG_H")

include_directories(${TINA_OUT_TARGET}/usr/include
		${TINA_DIR}/external/awcast/src/tinyui
		${TINA_DIR}/package/allwinner/liballwinner-base
		${TINA_DIR}/external/awcast/src/dlna/include
		${TINA_DIR}/external/miracast2/src/libwfd2
		${TINA_DIR}/external/miracast2/src/libwfd2/include
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/xplayer/include
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/libcore/playback/include
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarc/include
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/external/include/adecoder
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/libcore/common/iniparser
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/libcore/parser/include
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/libcore/stream/include
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/libcore/base/include
		${TINA_DIR}/package/allwinner/tina_multimedia/libcedarx/libcore/base/include )

include_directories(${CMAKE_SOURCE_DIR})

link_directories(
		${TINA_DIR}/external/awcast/src/dlna/lib
		${TINA_OUT_TARGET}/rootfs/usr/lib
		)

add_subdirectory(tinyui)

add_executable( awcast 	awcast.c
			networkd-api.c
			ubus.c
			awcast_main_ui.c
			network_monitor.c
			wireless_display.c
			ini_config.c
			net_key.c )

target_link_libraries ( awcast
		tinyui aw_dlna
		)

target_link_libraries ( awcast
			wfd2 wfd_base wfd_player wfd_rtsp wpa_client
			xplayer cdx_playback cdx_stream cdx_parser
                        cdx_base  cdx_common
                        cdc_base  MemAdapter vdecoder
                        videoengine VE
                        adecoder subdecoder ssl crypto
                        cdx_sink_tina asound uci ubox
			minigui_ths pthread jpeg png m freetype z png12 freetype pthread bz2 ts
			ubus ubox uci blobmsg_json json-c uapi
			)

install(PROGRAMS awcast DESTINATION bin)
