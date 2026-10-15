#include "live_controls.h"

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
    if (live_controls_hit_test(10, 96) != LIVE_CONTROL_EXIT)
        return 1;
    if (live_controls_hit_test(69, 159) != LIVE_CONTROL_EXIT)
        return 1;
    if (live_controls_hit_test(10, 320) != LIVE_CONTROL_GUIDES)
        return 1;
    if (live_controls_hit_test(10, 208) != LIVE_CONTROL_FILE_MANAGER)
        return 1;
    if (live_controls_hit_test(730, 96) != LIVE_CONTROL_RECORD)
        return 1;
    if (live_controls_hit_test(789, 379) != LIVE_CONTROL_PHOTO)
        return 1;
    if (live_controls_hit_test(730, 208) != LIVE_CONTROL_VIEW_RECORDINGS)
        return 1;
    if (live_controls_hit_test(70, 96) != LIVE_CONTROL_NONE)
        return 1;
    if (live_controls_hit_test(730, 162) != LIVE_CONTROL_NONE)
        return 1;
    if (live_controls_hit_test(730, 380) != LIVE_CONTROL_NONE)
        return 1;
    if (live_controls_hit_test(400, 240) != LIVE_CONTROL_NONE)
        return 1;
    return 0;
}
