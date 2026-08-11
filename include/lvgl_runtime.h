#ifndef LVGL_RUNTIME_H
#define LVGL_RUNTIME_H

#include <stdint.h>

#include <lvgl.h>

#include "lcd.h"
#include "touch.h"

typedef struct {
    Lcd *lcd;
    Touch *touch;
    lv_display_t *display;
    lv_indev_t *input;
    uint8_t *draw_buffer;
    int lvgl_started;
    int initialized;
} LvglRuntime;

int lvgl_runtime_init(LvglRuntime *runtime, Lcd *lcd, Touch *touch);
void lvgl_runtime_destroy(LvglRuntime *runtime);
void lvgl_runtime_process(LvglRuntime *runtime);
void lvgl_runtime_set_input_enabled(LvglRuntime *runtime, int enabled);
lv_display_t *lvgl_runtime_display(const LvglRuntime *runtime);

#endif
