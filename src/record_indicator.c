#include "app_config.h"
#include "record_indicator.h"

#define RECORD_COLOR 0xFF0000
#define BACKGROUND_COLOR 0x000000

//最新改动
// 在坐标合法时写入一个 framebuffer 像素。
static void draw_pixel(int *lcd_mp, int x, int y, int color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT)
        lcd_mp[y * LCD_WIDTH + x] = color;
}
//最新改动

//最新改动
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
//最新改动

//最新改动
// 绘制或清除右上角固定显示的 REC 字样。
void record_indicator_draw_rec(int *lcd_mp, int visible)
{
    const int color = visible ? RECORD_COLOR : BACKGROUND_COLOR;
    const int top = 22;
    const int bottom = 38;

    for (int y = top; y <= bottom; ++y)
    {
        for (int x = 756; x <= 764; ++x)
        {
            if (x == 756 || y == top || y == 30 ||
                (x == 764 && y <= 30) || (y > 30 && x == y + 734))
                draw_pixel(lcd_mp, x, y, color);
        }
        for (int x = 770; x <= 778; ++x)
        {
            if (x == 770 || y == top || y == 30 || y == bottom)
                draw_pixel(lcd_mp, x, y, color);
        }
        for (int x = 784; x <= 792; ++x)
        {
            if (x == 784 || y == top || y == bottom)
                draw_pixel(lcd_mp, x, y, color);
        }
    }
}
//最新改动
