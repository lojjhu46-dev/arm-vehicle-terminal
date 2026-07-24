PROJECT := ./
INCLUDE := $(PROJECT)/include
LIB := $(PROJECT)/lib
BUILD := $(PROJECT)/build
BIN := $(PROJECT)/bin
SRC := $(PROJECT)/src

CC := arm-linux-gcc
TARGET := $(BIN)/app

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

CPPFLAGS := -I$(INCLUDE)
CFLAGS := -Wall -Wextra -std=gnu11 -pthread
LDFLAGS := -L$(LIB) -Wl,-rpath-link,$(LIB)
LDLIBS := -ljpeg -pthread

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD) $(BIN):
	mkdir -p $@

clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.d 

-include $(DEPS)