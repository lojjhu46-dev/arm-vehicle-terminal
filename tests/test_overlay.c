#include "overlay.h"

#define LCD_WIDTH 800
#define LCD_HEIGHT 480

int main(void)
{
    int framebuffer[LCD_WIDTH * LCD_HEIGHT] = {0};

    if (parking_assist_lines_drawing(framebuffer) != 0)
        return 1;

    if (framebuffer[470 * LCD_WIDTH + 30] != 0xFF0000)
        return 1;

    return 0;
}
