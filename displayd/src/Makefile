TARGET = displayd

SRCDIRS   =  $(shell find . -maxdepth 1 -type d)

INCLUDES	:=  $(foreach dir,$(SRCDIRS),-I$(dir))

LOCAL_CFLAGS = -std=c++11
LOCAL_LDFLAGS = -lpthread -lcutils -lsysutils

SRCS = $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
OBJS = $(patsubst %.cpp, %.cpp.o, $(SRCS))

all:install

$(TARGET):$(OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(LOCAL_CFLAGS) $(LOCAL_LDFLAGS) -o $@ $^

$(OBJS):%.cpp.o:%.cpp
	$(CXX) $(INCLUDES) $(CFLAGS) $(LDFLAGS) $(LOCAL_CFLAGS) $(LOCAL_LDFLAGS) -c $^ -o $@

install:$(TARGET)
	@mkdir -p $(CONFIG_PREFIX)/bin
	@cp $(TARGET) $(CONFIG_PREFIX)/bin

clean:
	-rm -rf $(OBJS) $(TARGET)
.PHONY:all clean $(TARGET) install
