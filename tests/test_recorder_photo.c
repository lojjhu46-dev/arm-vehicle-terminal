#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "jpeg_codec.h"
#include "recorder.h"
#include "app_config.h"

static char written_path[256];
static JSAMPLE image[CAMERA_FRAME_W * CAMERA_FRAME_H * 3];

void write_JPEG_file(const char *filename, int quality, const JSAMPLE *image_buffer)
{
    (void)quality;
    (void)image_buffer;
    snprintf(written_path, sizeof(written_path), "%s", filename);
}

int main(void)
{
    Recorder recorder;
    struct stat status;

    if (recorder_init(&recorder) != 0)
        return 1;
    if (recorder_save_photo(&recorder, "/tmp/mainui-photo-test", image) != 0)
        return 1;
    recorder_wait_idle(&recorder);
    if (stat("/tmp/mainui-photo-test", &status) != 0 || !S_ISDIR(status.st_mode))
        return 1;
    if (strncmp(written_path, "/tmp/mainui-photo-test/photo_", 29) != 0)
        return 1;
    recorder_destroy(&recorder);
    return 0;
}
