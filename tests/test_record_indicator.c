#include "record_indicator.h"

#define LCD_WIDTH 800
#define LCD_HEIGHT 480
#define REC_WIDTH 49
#define REC_HEIGHT 26

static int rec_pixels[REC_WIDTH * REC_HEIGHT];

int read_JPEG_image(const char *filename, int **pixels, int *width, int *height)
{
    (void)filename;
    (void)pixels;
    (void)width;
    (void)height;
    return -1;
}

int main(void)
{
    int framebuffer[LCD_WIDTH * LCD_HEIGHT] = {0};
    RecordIndicator indicator = {
        .pixels = rec_pixels,
        .width = REC_WIDTH,
        .height = REC_HEIGHT,
    };

    for (int i = 0; i < REC_WIDTH * REC_HEIGHT; ++i)
        rec_pixels[i] = 0xFF0000;

    record_indicator_draw_dot(framebuffer, 1);
    record_indicator_draw_rec(&indicator, framebuffer, 1);
    if (framebuffer[30 * LCD_WIDTH + 740] != 0xFF0000)
        return 1;
    if (framebuffer[17 * LCD_WIDTH + 750] != 0xFF0000)
        return 1;

    record_indicator_draw_dot(framebuffer, 0);
    record_indicator_draw_rec(&indicator, framebuffer, 0);
    if (framebuffer[30 * LCD_WIDTH + 740] != 0x000000)
        return 1;
    if (framebuffer[17 * LCD_WIDTH + 750] != 0x000000)
        return 1;
    return 0;
}
