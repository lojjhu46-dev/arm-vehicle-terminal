#include <string.h>

#include "app_config.h"
#include "image_convert.h"

static char yuyv[CAMERA_FRAME_W * CAMERA_FRAME_H * 2];
static unsigned int framebuffer[LCD_WIDTH * LCD_HEIGHT];

int main(void)
{
    ImageConverter converter;

    for (size_t i = 0; i < sizeof(yuyv); i += 4)
    {
        yuyv[i] = 16;
        yuyv[i + 1] = 128;
        yuyv[i + 2] = 16;
        yuyv[i + 3] = 128;
    }
    for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i)
        framebuffer[i] = 0x123456;

    if (image_converter_init(&converter) != 0)
        return 1;
    if (image_convert_frame(&converter, yuyv, framebuffer) == NULL)
        return 1;
    if (framebuffer[0] != 0x123456 ||
        framebuffer[200 * LCD_WIDTH + CAMERA_DISPLAY_X - 1] != 0x123456 ||
        framebuffer[200 * LCD_WIDTH + CAMERA_DISPLAY_X + CAMERA_FRAME_W] != 0x123456 ||
        framebuffer[CAMERA_DISPLAY_X] == 0x123456)
        return 1;

    image_converter_destroy(&converter);
    return 0;
}
