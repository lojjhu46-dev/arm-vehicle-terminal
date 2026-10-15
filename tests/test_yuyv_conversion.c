#include "image_convert.h"

int main(void)
{
    JSAMPLE rgb[3] = {0};
    JSAMPLE *output = rgb;
    unsigned int pixel = yuyv_to_rgb_pix(0, 128, 128, &output);

    if (pixel != 0)
        return 1;
    if (rgb[0] != 0 || rgb[1] != 0 || rgb[2] != 0)
        return 1;

    return 0;
}
