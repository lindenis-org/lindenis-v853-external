SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=%.o)
DEPEND := $(SRCS:%.c=%.d)

C_FLAGS += $(CFLAGS) -fPIC
LD_FLAGS += $(LDFLAGS) -L./

TARGET := networkd
NETWORKD_OBJS = networkd.o wifi_manager.o ubus.o

LIB_TARGET := libnetworkd.so
LIB_OBJS := ubus.o wifi_manager.o networkd_api.o

CLIENT_TARGET := networkd_client
CLIENT_OBJS := client.o
#LD_FLAGS = -lpthread -lm -lubus -lubox \
#		-lpthread -luci -lblobmsg_json -ljson-c\
#		-lmqttd-svc-api \
#		-L$(STAGING_DIR)/usr/lib


.PHONY: all clean

all:$(TARGET) $(LIB_TARGET) $(CLIENT_TARGET)

-include $(DEPEND)

$(DEPEND):$(SRCS)
	$(CC) $(C_FLAGS) -MM $(SRCS) > $@.$$$$ ;\
	sed 's/$(OBJS)/$(OBJS) $@/g' < $@.$$$$ >$@ ;\
	$(RM) $@.$$$$

%.o:%.c
	$(CC) -c $(C_FLAGS) $<

$(TARGET):$(NETWORKD_OBJS)
	$(CC) $(LD_FLAGS) $^ -o $@

$(LIB_TARGET):$(LIB_OBJS)
	$(CC) $(LD_FLAGS) $^ -o $(LIB_TARGET)  -shared

$(CLIENT_TARGET):$(CLIENT_OBJS)
	$(CC) $(LD_FLAGS) -lnl-tiny -lnetworkd $^ -o $(CLIENT_TARGET)

clean:
	$(RM) $(TARGET) $(OBJS) $(DEPEND) $(LIB_TARGET)
