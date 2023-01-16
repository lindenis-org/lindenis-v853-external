
include $(top_srcdir)/Makefile.inc

bin_PROGRAMS = xplayerdemo2

xplayerdemo2_SOURCES =  ./xplayerdemo2.c 

xplayerdemo2_CFLAGS = $(CFLAGS_CDXG)
#xplayerdemo_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/xplayer/include  \
		-I$(top_srcdir)/platform/tina  \
		-I$(top_srcdir)/libcore/common/iniparser \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/stream/include \
		-I$(top_srcdir)/libcore/playback/include \
		-I$(top_srcdir)/libcore/parser/include \
		-I$(top_srcdir)/external/include/adecoder \
		-I$(top_srcdir)/external/include/   \
		-I$(top_srcdir)/../libcedarc/include

xplayerdemo2_CFLAGS += $(LOCAL_INCLUDE)
#xplayerdemo_CPPFLAGS += $(LOCAL_INCLUDE)

xplayerdemo2_LDFLAGS = $(LDFLAGS_CDXG)

xplayerdemo2_LDADD = $(top_srcdir)/libcore/parser/base/libcdx_parser.la \
			$(top_srcdir)/libcore/base/libcdx_base.la \
			$(top_srcdir)/libcore/stream/base/libcdx_stream.la \
			$(top_srcdir)/libcore/playback/libcdx_playback.la \
			$(top_srcdir)/xplayer/libxplayer.la \
			$(top_srcdir)/platform/tina/libcdx_sink_tina.la \
			$(top_srcdir)/libcore/common/libcdx_common.la

LOCAL_LIB = -ldl -lrt -lpthread -lz -lm -lvdecoder -ladecoder -lm -lsubdecoder -lVE \
		-lcrypto -lMemAdapter -lcdc_base -lssl -lvideoengine -lasound
#-lasound
#-lsalsa
xplayerdemo2_LDFLAGS += $(LOCAL_LIB)

