#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "record_indicator.h"

//最新修改
#define RECORD_COLOR 0xFF0000
#define BACKGROUND_COLOR 0x000000
#define REC_X 750
#define REC_Y 17
#define REC_WIDTH 49
#define REC_HEIGHT 26

// 在坐标合法时写入 framebuffer 像素。
static void draw_pixel(int *lcd_mp, int x, int y, int color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT)
        lcd_mp[y * LCD_WIDTH + x] = color;
}

// 加载 REC.jpeg，避免手绘字符变形。
int record_indicator_init(RecordIndicator *indicator)
{
    memset(indicator, 0, sizeof(*indicator));
    if (read_JPEG_image(RESOURCE_DIR "/REC.jpeg", &indicator->pixels,
                        &indicator->width, &indicator->height) == -1)
        return -1;
    if (indicator->width != REC_WIDTH || indicator->height != REC_HEIGHT)
    {
        free(indicator->pixels);
        memset(indicator, 0, sizeof(*indicator));
        return -1;
    }
    return 0;
}

// 释放 REC 图标缓存。
void record_indicator_destroy(RecordIndicator *indicator)
{
    free(indicator->pixels);
    memset(indicator, 0, sizeof(*indicator));
}

// 绘制或清除右上角录像闪烁红点。
void record_indicator_draw_dot(int *lcd_mp, int visible)
{
    const int center_x = 740;
    const int center_y = 30;
    const int radius = 6;
    const int color = visible ? RECORD_COLOR : BACKGROUND_COLOR;

    for (int y = center_y - radius; y <= center_y + radius; ++y)
    {
        for (int x = center_x - radius; x <= center_x + radius; ++x)
        {
            int dx = x - center_x;
            int dy = y - center_y;

            if (dx * dx + dy * dy <= radius * radius)
                draw_pixel(lcd_mp, x, y, color);
        }
    }
}

// 绘制或清除 JPEG 形式的 REC 图标。
void record_indicator_draw_rec(const RecordIndicator *indicator, int *lcd_mp,
                               int visible)
{
    for (int y = 0; y < REC_HEIGHT; ++y)
    {
        for (int x = 0; x < REC_WIDTH; ++x)
        {
            int color = BACKGROUND_COLOR;

            if (visible && indicator != NULL && indicator->pixels != NULL)
                color = indicator->pixels[y * indicator->width + x];
            draw_pixel(lcd_mp, REC_X + x, REC_Y + y, color);
        }
    }
}
//最新修改
