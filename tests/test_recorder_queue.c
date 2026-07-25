#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "recorder.h"

static JSAMPLE image[CAMERA_FRAME_W * CAMERA_FRAME_H * 3];

void write_JPEG_file(const char *filename, int quality, const JSAMPLE *image_buffer)
{
    (void)filename;
    (void)quality;
    (void)image_buffer;
    usleep(100 * 1000);
}

int main(void)
{
    Recorder recorder;

    if (mkdir("/tmp/mainui-record-queue", 0777) != 0 && errno != EEXIST)
        return 1;
    if (recorder_init(&recorder) != 0)
        return 1;
    if (recorder_start_session(&recorder, "/tmp/mainui-record-queue") != 0)
        return 1;
    for (int i = 0; i < RECORDER_QUEUE_DEPTH; ++i)
    {
        if (recorder_submit_frame(&recorder, image) != 0)
            return 1;
    }
    if (recorder_submit_frame(&recorder, image) != 1 ||
        recorder_dropped_frames(&recorder) != 1)
        return 1;

    recorder_stop_session(&recorder);
    recorder_wait_idle(&recorder);
    recorder_destroy(&recorder);
    return 0;
}
