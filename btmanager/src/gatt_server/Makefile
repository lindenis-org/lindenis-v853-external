include $(TOPDIR)/rules.mk

target = libgatt-server.so
install:
	-@mkdir -p $(CONFIG_PREFIX)/usr/lib
	-@mkdir -p $(CONFIG_PREFIX)/usr/bin
	-@mkdir -p $(CONFIG_PREFIX)/usr/include
	-@cp -rf ./include/* $(CONFIG_PREFIX)/usr/include

ifeq ($(CONFIG_ARCH_64BIT),y)
	-@cp ./$(CONFIG_LIBC)/arm64/*.a $(CONFIG_PREFIX)/usr/lib
else
	-@cp ./$(CONFIG_LIBC)/arm/*.a $(CONFIG_PREFIX)/usr/lib
endif

INCLUDES +=-I./include -I$(STAGING_DIR)/usr/include/glib-2.0 \
	-I$(STAGING_DIR)/usr/include/gio-unix-2.0 -I$(CONFIG_PREFIX)/usr/include

LIBS = -L$(CONFIG_PREFIX)/usr/lib -lbluetooth-internal -lshared-mainloop


LIB_SRC = $(wildcard *.c)

$(target):$(LIB_SRC)
	$(CC) -fPIC -shared $(CFLAGS) $(INCLUDES) $(CPPFLAGS) $(LDFLAGS) $(LIBS) $^ -o $@
	@cp $@  $(CONFIG_PREFIX)/usr/lib
ifeq ($(CONFIG_ARCH_64BIT),y)
	@cp ./$(CONFIG_LIBC)/arm64/*.a $(CONFIG_PREFIX)/usr/lib
else
	@cp ./$(CONFIG_LIBC)/arm/*.a $(CONFIG_PREFIX)/usr/lib
endif

all:install $(target)

clean:
	rm -f $(OBJS) libgatt-server.so
