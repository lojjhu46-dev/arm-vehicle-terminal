CC := arm-linux-gcc
CFLAGS ?= -Wall -Wextra -std=gnu11
CPPFLAGS := -Iinclude
LDFLAGS := -L$(CURDIR)/lib
LDLIBS := -ljpeg
THREAD_FLAGS := -pthread

APP_SOURCES := src/main.c src/lcd.c src/touch.c src/camera.c src/image_convert.c src/overlay.c src/jpeg_codec.c src/recorder.c src/ui_flow.c

.PHONY: all test-overlay test-public-headers test-recorder-header test-recorder-session test-yuyv-conversion syntax-check

all: mainui

mainui: $(APP_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(THREAD_FLAGS) -L$(CURDIR)/lib -Wl,-rpath-link,$(CURDIR)/lib -o $@ $(APP_SOURCES) $(LDLIBS)
test-overlay: tests/test_overlay.c src/overlay.c include/overlay.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o tests/test_overlay tests/test_overlay.c src/overlay.c
	./tests/test_overlay

test-public-headers: tests/test_public_headers.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only tests/test_public_headers.c

test-recorder-header: tests/test_recorder_header.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only tests/test_recorder_header.c

test-recorder-session: tests/test_recorder_session.c src/recorder.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o tests/test_recorder_session tests/test_recorder_session.c src/recorder.c
	./tests/test_recorder_session

test-yuyv-conversion: tests/test_yuyv_conversion.c src/image_convert.c src/overlay.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o tests/test_yuyv_conversion tests/test_yuyv_conversion.c src/image_convert.c src/overlay.c
	./tests/test_yuyv_conversion

syntax-check:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $(APP_SOURCES)
