PROJECT := ./
INCLUDE := $(PROJECT)/include
LIB := $(PROJECT)/lib
BUILD := $(PROJECT)/build
BIN := $(PROJECT)/bin
SRC := $(PROJECT)/src
LVGL_ROOT ?= $(PROJECT)/../lvgl_project/lv_port_pc_vscode/lvgl
FREETYPE_ROOT ?= $(PROJECT)/../freetype/freetype_arm

CC := arm-linux-gcc
TARGET := $(BIN)/app

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))
LVGL_SRCS := $(shell find $(LVGL_ROOT)/src -type f -name '*.c')
LVGL_OBJS := $(patsubst $(LVGL_ROOT)/%.c,$(BUILD)/lvgl/%.o,$(LVGL_SRCS))
OBJS += $(LVGL_OBJS)
DEPS := $(OBJS:.o=.d)

CPPFLAGS := -I$(INCLUDE) -I$(LVGL_ROOT) -I$(FREETYPE_ROOT)/include -DLV_CONF_INCLUDE_SIMPLE
CFLAGS := -Wall -Wextra -std=gnu11 -pthread
LDFLAGS := -L$(LIB) -Wl,-rpath-link,$(LIB)
LDLIBS := -ljpeg -pthread -lm $(FREETYPE_ROOT)/lib/libfreetype.a -ldl

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/lvgl/%.o: $(LVGL_ROOT)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD) $(BIN):
	mkdir -p $@

clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.d 

-include $(DEPS)
