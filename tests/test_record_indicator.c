#include "record_indicator.h"

#define LCD_WIDTH 800
#define LCD_HEIGHT 480

//最新改动
// 验证录像指示器在亮起和清除时写入预期颜色。
int main(void)
{
    int framebuffer[LCD_WIDTH * LCD_HEIGHT] = {0};

    record_indicator_draw_dot(framebuffer, 1);
    record_indicator_draw_rec(framebuffer, 1);
    if (framebuffer[30 * LCD_WIDTH + 740] != 0xFF0000)
        return 1;
    if (framebuffer[30 * LCD_WIDTH + 770] != 0xFF0000)
        return 1;

    record_indicator_draw_dot(framebuffer, 0);
    record_indicator_draw_rec(framebuffer, 0);
    if (framebuffer[30 * LCD_WIDTH + 740] != 0x000000)
        return 1;
    if (framebuffer[30 * LCD_WIDTH + 770] != 0x000000)
        return 1;

    return 0;
}
//最新改动
