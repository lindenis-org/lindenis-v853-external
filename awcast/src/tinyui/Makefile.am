
lib_LTLIBRARIES = libtinyui.la

libtinyui_la_SOURCES = \
	./zmetadata.c \
	./minigui_paint.c \
	./tinyui.c

libtinyui_la_CFLAGS = -Werror -g

libtinyui_la_CFLAGS += \
	-I$(srcdir) \
	-I$(STAGING_DIR)/target/usr/include

libtinyui_la_CFLAGS += \
    -I$(TINA_DIR)/package/allwinner/tina_multimedia/libcedarx/libcore/common/iniparser \
    -I$(TINA_DIR)/package/allwinner/tina_multimedia/libcedarx/libcore/base/include

libtinyui_la_LDFLAGS = -avoid-version \
			-Wl,--no-undefined

libtinyui_la_LDFLAGS += \
	-L$(STAGING_DIR)/target/usr/lib \
	-lpthread \
	-lcdx_base \
	-lxml2 \
	-lminigui_ths -lts -lpng  -ljpeg -lz -lm -lfreetype -lbz2

bin_PROGRAMS = ut_tinyui

ut_tinyui_SOURCES = \
	./ut/ut_tinyui.c

ut_tinyui_CFLAGS = -I$(srcdir) \
	-I$(STAGING_DIR)/target/usr/include \
    	-I$(TINA_DIR)/package/allwinner/tina_multimedia/libcedarx/libcore/common/iniparser \
    	-I$(TINA_DIR)/package/allwinner/tina_multimedia/libcedarx/libcore/base/include

ut_tinyui_LDADD = \
	$(top_srcdir)/tinyui/libtinyui.la

ut_tinyui_LDFLAGS =  \
	-L$(STAGING_DIR)/target/usr/lib \
	-lpthread \
	-lcdx_base \
	-lxml2 \
	-lminigui_ths -lts -lpng  -ljpeg -lz -lm -lfreetype -lbz2

#-luapi -lrt -lc

