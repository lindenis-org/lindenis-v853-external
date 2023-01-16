
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_sink_tina.la

libcdx_sink_tina_la_SOURCES = \
	./subtitleControl_null.c \
	./volume_uci_config_api.c  \
	./tinasoundcontrol.c \
	./CdxIon.c \
	./soundControl_alsa.c \
	./mixer.c \
	./deinterlace_null.c

if USE_LAYER_CONTROL2
libcdx_sink_tina_la_SOURCES += \
        ./layerControl_de2.c \
		./disputils2.c
else
libcdx_sink_tina_la_SOURCES += \
        ./layerControl_de.c \
		./disputils.c
endif


libcdx_sink_tina_la_CFLAGS = $(CFLAGS_CDXG)

LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/common/iniparser \
		-I$(top_srcdir)/libcore/playback/include \
		-I$(top_srcdir)/external/include/adecoder \
		-I$(top_srcdir)/external/include \
		-I$(top_srcdir)/../libcedarc/include

libcdx_sink_tina_la_CFLAGS += $(LOCAL_INCLUDE)

libcdx_sink_tina_la_LDFLAGS = $(LDFLAGS_CDXG) \
	-lpthread \
      -lcdx_base -lcdx_stream -lcdx_parser -lcdx_playback -lxplayer -lcdx_common \
      -lcdc_base -lMemAdapter -lvdecoder \
      -lvideoengine -lVE \
      -ladecoder -lsubdecoder -lssl -lcrypto -lz -lasound \
      -luci -lAudioGain

libcdx_sink_tina_la_LIBADD = \
			$(top_srcdir)/libcore/base/libcdx_base.la

