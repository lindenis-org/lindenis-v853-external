include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

target = libwifimg.so

INCLUDES        += -I. -I./include
INCLUDES        += -I./include/os
INCLUDES        += -I./include/os/linux
INCLUDES        += -I$(STAGING_DIR)/usr/include/libnl-tiny
INCLUDES        += -I$(STAGING_DIR)/lib/libwirelesscom.so
LIBS            += -lpthread -ldl -lrt -lnl-tiny

SRCS += src/wifimg.c
SRCS += src/wmg_common.c
SRCS += src/wmg_sta.c
SRCS += src/wmg_ap.c
SRCS += src/wmg_monitor.c
SRCS += src/log/wifi_log.c

SRCS += src/os/linux/udhcpc.c
SRCS += src/os/linux/event.c
SRCS += src/os/linux/linux_wpa.c
SRCS += src/os/linux/linux_hapd.c
SRCS += src/os/linux/linux_nl.c
SRCS += src/os/linux/linux_common.c
SRCS += src/os/linux/scan.c
SRCS += src/os/linux/utils.c

LOCAL_CFLAGS = -D_GNU_SOURCE=1
LOCAL_CFLAGS = -DOS_NET_LINUX_OS

install:
	-@mkdir -p $(CONFIG_PREFIX)/usr/lib
	-@mkdir -p $(CONFIG_PREFIX)/usr/include
	@cp ./include/*.h $(CONFIG_PREFIX)/usr/include

#libwifimg-v2.0.so
#ifdef CONFIG_LINUX_5_4 || CONFIG_LINUX_4_9
ifeq ($(CONFIG_ARCH), "aarch64")
    SRCS += ./src/os/linux/lib/libwpa_client_64.a
else
    ifeq ($(CONFIG_SOFT_FLOAT), y)
        SRCS += ./src/os/linux/lib/libwpa_client_softft.a
    else
         ifeq ($(TARGET_ARCH), riscv)
              SRCS += ./src/os/linux/lib/libwpa_client_riscv.a
         else
              SRCS += ./src/os/linux/lib/libwpa_client.a
         endif
    endif
endif
#endif
$(target): $(SRCS)
	$(CC) -fPIC -shared $(INCLUDES) $(LOCAL_CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)
	@cp $@ $(CONFIG_PREFIX)/usr/lib

all: install $(target)
clean:
	rm -rf $(target)
