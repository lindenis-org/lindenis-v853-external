include $(top_srcdir)/common.mk

######### libwfd2.so #########
lib_LTLIBRARIES = libwfd2.la

libwfd2_la_SOURCES = \
    miracast2.cpp \
    wpa_cli.c

libwfd2_la_CFLAGS = \
    -I$(srcdir) \
    -I$(srcdir)/include \
    -I$(top_srcdir)/WFDBase \
    -I$(top_srcdir)/WFDPlayer \
    -I$(top_srcdir)/WFDSink/UsageEnvironment/include \
    -I$(top_srcdir)/WFDSink/groupsock/include \
    -I$(top_srcdir)/WFDSink/liveMedia/include \
    -I$(top_srcdir)/WFDSink/BasicUsageEnvironment/include \
	-I$(STAGING_DIR)/target/usr/include/allwinner/include \
	-I$(STAGING_DIR)/target/usr/include/allwinner \
	-I$(STAGING_DIR)/target/usr/include

libwfd2_la_CFLAGS += -MMD -O2 -Wall -g \
    -DCONFIG_CTRL_IFACE \
    -DCONFIG_CTRL_IFACE_UNIX \
    -DCONFIG_P2P \
    -DCONFIG_WIFI_DISPLAY \
    -DCONFIG_WPS \
    -DCONFIG_WPS_NFC \
    -DCONFIG_AP \
    -DCONFIG_HS20 \
    -DCONFIG_AUTOSCAN \
    -DCONFIG_INTERWORKING \
    -DIEEE8021X_EAPOL

libwfd2_la_CPPFLAGS = \
    -I$(srcdir) \
    -I$(top_srcdir)/WFDBase \
    -I$(top_srcdir)/WFDPlayer \
    -I$(top_srcdir)/WFDSink/UsageEnvironment/include \
    -I$(top_srcdir)/WFDSink/groupsock/include \
    -I$(top_srcdir)/WFDSink/liveMedia/include \
    -I$(top_srcdir)/WFDSink/BasicUsageEnvironment/include \
	-I$(STAGING_DIR)/target/usr/include/allwinner/include \
	-I$(STAGING_DIR)/target/usr/include/allwinner \
	-I$(STAGING_DIR)/target/usr/include

libwfd2_la_LDFLAGS = $(WFD_COMMON_LDFLAGS)

libwfd2_la_LDFLAGS +=  \
    -L$(top_srcdir)/library -lwpa_client

libwfd2_la_LDFLAGS += -lpthread \
    -L$(STAGING_DIR)/target/usr/lib \
    -lcdx_base  -lcdx_common \
    -lcdc_base  -lMemAdapter -lvdecoder \
    -lvideoengine -lVE \
    -ladecoder -lsubdecoder -lssl -lcrypto -lz \
    -lcdx_sink_tina -lasound -luci -lubox

libwfd2_la_LIBADD = \
    $(top_srcdir)/WFDSink/libwfd_rtsp.la \
    $(top_srcdir)/WFDPlayer/libwfd_player.la \
    $(top_srcdir)/WFDBase/libwfd_base.la

######### test ##########
bin_PROGRAMS = miracast2

miracast2_SOURCES = \
    main_test.c

miracast2_CFLAGS = \
    -I$(top_srcdir) \
	-I$(STAGING_DIR)/target/usr/include/allwinner/include \
	-I$(STAGING_DIR)/target/usr/include/allwinner \
	-I$(STAGING_DIR)/target/usr/include

miracast2_LDFLAGS =  \
    -L$(top_srcdir)/library -lwpa_client

miracast2_LDFLAGS += \
    -lpthread \
    -L$(STAGING_DIR)/target/usr/lib \
    -lcdx_base  -lcdx_common \
    -lcdc_base  -lMemAdapter -lvdecoder \
    -lvideoengine -lVE \
    -ladecoder -lsubdecoder -lssl -lcrypto -lz

if H133
miracast2_LDFLAGS += -lfbm -laftertreatment -lscaledown -lsbm
endif

miracast2_LDFLAGS += -lcdx_sink_tina -lasound -luci -lubox

miracast2_LDADD = \
    $(top_srcdir)/WFDSink/libwfd_rtsp.la \
    $(top_srcdir)/WFDPlayer/libwfd_player.la \
    $(top_srcdir)/WFDBase/libwfd_base.la \
    $(srcdir)/libwfd2.la

