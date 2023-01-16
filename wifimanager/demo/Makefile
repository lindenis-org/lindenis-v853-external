include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

INCLUDES += -I $(CONFIG_PREFIX)/usr/include \
			-L $(STAGING_DIR)/lib/ \
			-I $(STAGING_DIR)/usr/include \
			-I ../core/include \
			-I ../core/include/linux \
			-I ./linkd/lib \
			-I ./linkd/ \

install:
	-@mkdir -p $(CONFIG_PREFIX)/usr/bin

DEAMON_LIBS += -lwifimg
DEAMON_LIBS += -lwirelesscom

DEAMON_SRCS += wifi_daemon.c
DEAMON_SRCS += linkd/linkd.c

ifdef CONFIG_WMG_PROTOCOL_SOFTAP
DEAMON_SRCS += linkd/linkd_softap/linkd_softap.c
endif

ifdef CONFIG_WMG_PROTOCOL_BLE
DEAMON_SRCS += linkd/linkd_ble/linkd_ble.c
DEAMON_LIBS += -lbtmg
DEAMON_LIBS += -lbluetooth
DEAMON_LIBS += -lbluetooth-internal
DEAMON_LIBS += -lglib-2.0 -lsbc -ljson-c -lgio-2.0 -lgobject-2.0 -lasound -lbtmg -ldl -lm -lresolv
DEAMON_LIBS += -lshared-mainloop
DEAMON_LIBS += -ldbus-1
endif

ifdef CONFIG_WMG_PROTOCOL_XCONFIG
INCLUDES += -I ./linkd/lib/lib_xconfig
DEAMON_SRCS += linkd/linkd_xconfig/linkd_xconfig.c
DEAMON_LIBS += -lcrypto
ifeq ($(CONFIG_ARCH), "aarch64")
    DEAMON_SRCS += linkd/lib/lib_xconfig/libdecode_64.a
else
    ifeq ($(TARGET_ARCH), "riscv")
        DEAMON_SRCS += linkd/lib/lib_xconfig/libdecode_riscv.a
    else
        DEAMON_SRCS += linkd/lib/lib_xconfig/libdecode.a
    endif
endif
endif

ifdef CONFIG_WMG_PROTOCOL_SOUNDWAVE
INCLUDES += -I ./linkd/lib/lib_soundwave
DEAMON_LIBS += -lasound -luci
DEAMON_SRCS += linkd/linkd_soundwave/linkd_soundwave.c
ifeq ($(CONFIG_LIBC), "glibc")
    DEAMON_LIBS += -lm
ifeq ($(CONFIG_ARCH), "aarch64")
    ADTLIB=linkd/lib/lib_soundwave/glibc/aarch64/libADT.a
else
    ifeq ($(TARGET_ARCH), "riscv")
        ADTLIB=linkd/lib/lib_soundwave/glibc/riscv/libADT.a
    else
        ADTLIB=linkd/lib/lib_soundwave/glibc/arm/libADT.a
    endif
endif
else
ifeq ($(CONFIG_ARCH), "aarch64")
    ADTLIB=linkd/lib/lib_soundwave/musl/aarch64/libADT.a
else
    ifeq ($(TARGET_ARCH), "riscv")
        ADTLIB=linkd/lib/lib_soundwave/musl/riscv/libADT.a
    else
        ADTLIB=linkd/lib/lib_soundwave/musl/arm/libADT.a
    endif
endif
endif
endif

wifi_daemon: $(DEAMON_SRCS) $(ADTLIB)
	$(CC) -o $@ $^ $(CFLAGS) $(INCLUDES)  $(LDFLAGS) -L$(CONFIG_PREFIX)/usr/lib -L$(CONFIG_PREFIX)/lib $(DEAMON_LIBS)
	@cp -f wifi_daemon $(CONFIG_PREFIX)/usr/bin
wifi: wifi.c
	$(CC) -o $@ $^ $(CFLAGS) $(INCLUDES)  $(LDFLAGS) -L$(CONFIG_PREFIX)/usr/lib -lwifimg -lwirelesscom
	@cp -f wifi $(CONFIG_PREFIX)/usr/bin

all: install wifi_daemon wifi

clean:
	rm -rf wifi_daemon wifi
