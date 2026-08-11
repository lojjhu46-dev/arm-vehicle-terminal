#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app_config.h"
#include "lvgl_runtime.h"

#define LVGL_DRAW_BUFFER_BYTES (LCD_WIDTH * LCD_HEIGHT * 3)

static uint32_t runtime_tick_get(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint32_t)(now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL);
}

static void runtime_flush_cb(lv_display_t *display, const lv_area_t *area,
                             uint8_t *color_p)
{
    LvglRuntime *runtime = lv_display_get_user_data(display);
    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;

    if (runtime != NULL && runtime->lcd != NULL && width > 0 && height > 0 &&
        lcd_begin_frame(runtime->lcd) == 0)
    {
        for (int y = 0; y < height; ++y)
        {
            const uint8_t *source = color_p + (size_t)y * (size_t)width * 3;
            int *target = runtime->lcd->pixels +
                          (area->y1 + y) * LCD_WIDTH + area->x1;

            for (int x = 0; x < width; ++x)
                target[x] = source[x * 3 + 2] << 16 |
                            source[x * 3 + 1] << 8 |
                            source[x * 3];
        }
        (void)lcd_present(runtime->lcd);
    }
    lv_display_flush_ready(display);
}

static void runtime_input_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LvglRuntime *runtime = lv_indev_get_user_data(indev);
    int x = 0;
    int y = 0;
    int pressed = 0;

    if (runtime == NULL || runtime->touch == NULL ||
        touch_poll_pointer(runtime->touch, &x, &y, &pressed) == -1)
    {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->point.x = x;
    data->point.y = y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int lvgl_runtime_init(LvglRuntime *runtime, Lcd *lcd, Touch *touch)
{
    if (runtime == NULL || lcd == NULL || touch == NULL)
        return -1;
    memset(runtime, 0, sizeof(*runtime));
    runtime->draw_buffer = malloc(LVGL_DRAW_BUFFER_BYTES);
    if (runtime->draw_buffer == NULL)
        return -1;
    lv_init();
    runtime->lvgl_started = 1;
    lv_tick_set_cb(runtime_tick_get);
    runtime->display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    if (runtime->display == NULL)
    {
        fprintf(stderr, "[LVGL] display creation failed\n");
        goto failed;
    }
    lv_display_set_color_format(runtime->display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(runtime->display, runtime->draw_buffer, NULL,
                           LVGL_DRAW_BUFFER_BYTES, LV_DISPLAY_RENDER_MODE_FULL);
    runtime->lcd = lcd;
    runtime->touch = touch;
    lv_display_set_user_data(runtime->display, runtime);
    lv_display_set_flush_cb(runtime->display, runtime_flush_cb);
    runtime->input = lv_indev_create();
    if (runtime->input == NULL)
        goto failed;
    lv_indev_set_type(runtime->input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(runtime->input, runtime->display);
    lv_indev_set_user_data(runtime->input, runtime);
    lv_indev_set_read_cb(runtime->input, runtime_input_read_cb);
    runtime->initialized = 1;
    return 0;

failed:
    fprintf(stderr, "[LVGL] runtime initialization failed\n");
    lvgl_runtime_destroy(runtime);
    return -1;
}

void lvgl_runtime_destroy(LvglRuntime *runtime)
{
    if (runtime == NULL)
        return;
    if (runtime->input != NULL)
        lv_indev_delete(runtime->input);
    if (runtime->display != NULL)
        lv_display_delete(runtime->display);
    if (runtime->lvgl_started)
        lv_deinit();
    free(runtime->draw_buffer);
    memset(runtime, 0, sizeof(*runtime));
}

void lvgl_runtime_process(LvglRuntime *runtime)
{
    if (runtime != NULL && runtime->initialized)
        (void)lv_timer_handler();
}

void lvgl_runtime_set_input_enabled(LvglRuntime *runtime, int enabled)
{
    if (runtime != NULL && runtime->input != NULL)
        lv_indev_enable(runtime->input, enabled != 0);
}

lv_display_t *lvgl_runtime_display(const LvglRuntime *runtime)
{
    return runtime == NULL ? NULL : runtime->display;
}
