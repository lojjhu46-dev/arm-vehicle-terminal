# mainUI Modularization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split `build/mainUI.c` into focused source and header files while preserving its existing runtime behavior and retaining the original file.

**Architecture:** `main.c` owns startup and the user-facing state flow. Hardware access, JPEG codec, pixel conversion, overlay drawing, and recording state move into focused modules. Existing function bodies and Chinese comments are copied with only include and context-boundary changes.

**Tech Stack:** C, Linux framebuffer/input/V4L2 APIs, libjpeg, GNU Make.

---

### Task 1: Establish a compilation check

**Files:**
- Create: `Project/tests/test_overlay.c`
- Create: `Project/Makefile`

- [ ] **Step 1: Write the failing overlay test**

```c
#include "overlay.h"

int main(void) {
    int framebuffer[800 * 480] = {0};
    return parking_assist_lines_drawing(framebuffer) == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Run `make test-overlay` and verify that it fails because `overlay.h` does not exist.**

- [ ] **Step 3: Add the minimal header and build target.**

- [ ] **Step 4: Re-run `make test-overlay` after Task 2 and verify it passes.**

### Task 2: Extract independent image modules

**Files:**
- Create: `Project/include/app_config.h`
- Create: `Project/include/overlay.h`
- Create: `Project/include/jpeg_codec.h`
- Create: `Project/src/overlay.c`
- Create: `Project/src/jpeg_codec.c`

- [ ] **Step 1: Move parking-line drawing unchanged into `overlay.c`; preserve all existing comments.**
- [ ] **Step 2: Move JPEG read/write code into `jpeg_codec.c`; pass the RGB source buffer explicitly to the encoder.**
- [ ] **Step 3: Run `make test-overlay` and `make syntax-check`.**

### Task 3: Extract device and conversion modules

**Files:**
- Create: `Project/include/lcd.h`
- Create: `Project/include/touch.h`
- Create: `Project/include/camera.h`
- Create: `Project/include/image_convert.h`
- Create: `Project/include/recorder.h`
- Create: `Project/src/lcd.c`
- Create: `Project/src/touch.c`
- Create: `Project/src/camera.c`
- Create: `Project/src/image_convert.c`
- Create: `Project/src/recorder.c`

- [ ] **Step 1: Put each device's file descriptor and state in its own context structure.**
- [ ] **Step 2: Preserve V4L2, touch scaling, YUYV conversion, and recording sequence.**
- [ ] **Step 3: Run `make syntax-check`.**

### Task 4: Assemble the application flow

**Files:**
- Create: `Project/include/ui_flow.h`
- Create: `Project/src/ui_flow.c`
- Create: `Project/src/main.c`
- Modify: `Project/Makefile`

- [ ] **Step 1: Move animation, touch-driven start, capture loop, and replay flow into `ui_flow.c`.**
- [ ] **Step 2: Keep `main.c` to LCD lifecycle and invocation of `ui_run`.**
- [ ] **Step 3: Compile all new source files with `make syntax-check`; attempt `make` only when a usable libjpeg link target is available.**

### Task 5: Preserve source and verify scope

**Files:**
- Keep: `Project/build/mainUI.c`
- Keep: every existing `Project` file

- [ ] **Step 1: Confirm `build/mainUI.c` remains present and unchanged.**
- [ ] **Step 2: Confirm `make test-overlay` and `make syntax-check` pass.**
- [ ] **Step 3: Report any target-runtime checks that cannot run without `/dev/fb0`, `/dev/input/event0`, and `/dev/video7`.**
