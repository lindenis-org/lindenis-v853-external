ifneq ($V,1)
Q ?= @
endif

CC=$(shell find ~/* -name arm-openwrt-linux-muslgnueabi-gcc | grep toolchain-sunxi-musl | head -1)
CPP=$(shell find ~/* -name arm-openwrt-linux-muslgnueabi-g++ | grep toolchain-sunxi-musl | head -1)
LINK=$(shell find ~/* -name arm-openwrt-linux-muslgnueabi-ar | grep toolchain-sunxi-musl | head -1)



ifeq (${CC}, )
	$(error "Can not find gcc tools, please add arm-openwrt-linux-muslgnueabi-gcc in your build env")
endif

ifeq (${CPP}, )
	$(error "Can not find g++ tools, please add arm-openwrt-linux-muslgnueabi-g++ in your build env")
endif

ifeq (${LINK}, )
	$(error "Can not find ar tools, please add arm-openwrt-linux-muslgnueabi-ar in your build env")
endif

CFLAGS  := -Os -pipe -march=armv7-a -mtune=cortex-a7 -mfpu=neon -fno-caller-saves -Wno-unused-result -mfloat-abi=hard
CFLAGS  += -g -D_FILE_OFFSET_BITS=64  -Wformat -Werror=format-security -fstack-protector -D_FORTIFY_SOURCE=1 -Wl,-z,now -Wl,-z,relro
CFLAGS  += -std=gnu99 -c -s -ffunction-sections -fdata-sections

CPPFLAGS  := -Os -pipe -march=armv7-a -mtune=cortex-a7 -mfpu=neon -fno-caller-saves -Wno-unused-result -mfloat-abi=hard
CPPFLAGS  += -g -D_FILE_OFFSET_BITS=64  -Wformat -Werror=format-security -fstack-protector -D_FORTIFY_SOURCE=1 -Wl,-z,now -Wl,-z,relro
CPPFLAGS  += -fexceptions -std=c++11 -c -s -ffunction-sections -fdata-sections -fpermissive

DEFS += -D__ARM__
DEFS += -D__ARMV7__
DEFS += -DCONF_KERNEL_IOMMU
CFLAGS += $(DEFS)
CPPFLAGS += $(DEFS)

INCLUDE = -I./include

LDFLAGS	= -L/usr/local/lib  -lpthread
LDFLAGS += -L./3rdparty/lib -lyolov3  -L ../com_libs -lawipubsp -lawnn


SRC1 = ./src/main.cpp


TAGET1 = yolov3

OBJ = $(SRC1:.cpp=.o)


all:$(OBJ1) $(OBJ) 
	$Q echo [$(TAGET1) ]
	$Q $(CPP)  -o $(TAGET1)  $(OBJ)  $(INCLUDE) $(LDFLAGS)

.c.o:
	$Q echo [CC] $<
	$Q $(CC) -c $(CFLAGS) $< -o $@ $(INCLUDE) -Wno-strict-aliasing

.cpp.o:
	$Q echo [CPP] $<
	$Q $(CPP) -c $(CPPFLAGS) $< -o $@ $(INCLUDE) -Wno-strict-aliasing 

clean:
	$Q echo "[Clean]"
	$Q rm -f $(OBJ1) $(OBJ) *~ $(TAGET1) 


