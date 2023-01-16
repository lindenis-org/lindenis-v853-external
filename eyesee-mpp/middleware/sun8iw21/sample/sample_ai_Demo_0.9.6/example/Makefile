ifneq ($V,1)
Q ?= @
endif

DEBUG	= -O3 -fvisibility=hidden -s

# path to toolchain
CC=../../../toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc
CPP=../../../toolchain/bin/arm-openwrt-linux-muslgnueabi-g++

CFLAGS	 = $(DEBUG) -Wall -fPIC -std=c++0x -c -s -ffunction-sections -fdata-sections -fpermissive -march=armv7-a  -mtune=cortex-a7
CPPFLAGS = $(DEBUG) -Wall -fPIC -fexceptions -std=c++11 -c -s -ffunction-sections -fdata-sections -fpermissive -march=armv7-a  -mtune=cortex-a7

DEFS += -D__ARM__
DEFS += -D__ARMV7__
DEFS += -DCONF_KERNEL_IOMMU
DEFS += -DCONF_KERNEL_VERSION_4_9
CFLAGS += $(DEFS)
CPPFLAGS += $(DEFS)

INCLUDE	= -I/usr/local/include 
INCLUDE += -I./

LDFLAGS	= -L/usr/local/lib -lpthread
LDLIBS  = ./libawnn.a
LDLIBS += ./libawipubsp.a

SRC1 = $(wildcard ./*.cpp)

TAGET1 = AWNN_example

OBJ1 = $(SRC1:.cpp=.o)

all:$(OBJ1)
	$Q echo [$(TAGET1) ]
	$Q $(CPP) -o $(TAGET1)  $(OBJ1) $(LDLIBS)  $(INCLUDE) $(LDFLAGS)
.c.o:
	$Q echo [CC] $<
	$Q $(CC) -c $(CFLAGS) $< -o $@ $(INCLUDE) -Wno-strict-aliasing

.cpp.o:
	$Q echo [CPP] $<
	$Q $(CPP) -c $(CPPFLAGS) $< -o $@ $(INCLUDE) -Wno-strict-aliasing 

clean:
	$Q echo "[Clean]"
	$Q rm -f $(OBJ1)  *~ $(TAGET1) 
