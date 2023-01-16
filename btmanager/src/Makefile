include $(TOPDIR)/rules.mk

install:
	-@mkdir -p $(CONFIG_PREFIX)/usr/lib
	-@mkdir -p $(CONFIG_PREFIX)/usr/include
	@cp ./*.h $(CONFIG_PREFIX)/usr/include

ifeq ($(CONFIG_ARCH_64BIT),y)
	@echo ${CONFIG_ARCH}
	@cp ./$(CONFIG_LIBC)/arm64/libbtmg.so $(CONFIG_PREFIX)/usr/lib
	@cp ./$(CONFIG_LIBC)/arm64/libbttool.so $(CONFIG_PREFIX)/usr/lib
else
	@echo ${CONFIG_ARCH}
	@cp ./$(CONFIG_LIBC)/arm/libbtmg.so $(CONFIG_PREFIX)/usr/lib
	@cp ./$(CONFIG_LIBC)/arm/libbttool.so $(CONFIG_PREFIX)/usr/lib
endif

all:install

clean:
	rm -rf *.h *.a
