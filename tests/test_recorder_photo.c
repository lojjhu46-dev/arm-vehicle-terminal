#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "jpeg_codec.h"
#include "recorder.h"

static char written_path[256];

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

    recorder_init(&recorder);
    if (recorder_save_photo(&recorder, "/tmp/mainui-photo-test", NULL) != 0)
        return 1;
    if (stat("/tmp/mainui-photo-test", &status) != 0 || !S_ISDIR(status.st_mode))
        return 1;
    if (strncmp(written_path, "/tmp/mainui-photo-test/photo_", 29) != 0)
        return 1;
    return 0;
}
