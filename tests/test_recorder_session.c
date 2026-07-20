#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jpeg_codec.h"
#include "recorder.h"

void write_JPEG_file(const char *filename, int quality, const JSAMPLE *image_buffer)
{
    (void)filename;
    (void)quality;
    (void)image_buffer;
}

int main(void)
{
    Recorder recorder;
    time_t timestamp = 0;

    setenv("TZ", "UTC", 1);
    tzset();
    recorder_init(&recorder);

    if (recorder_prepare_session(&recorder, "/tmp/mainui-recordings", timestamp) != 0)
        return 1;
    if (strcmp(recorder.dirbuffer, "/tmp/mainui-recordings/1970-01-01_00-00-00") != 0)
        return 1;

    recorder.recording = 1;
    recorder_stop_session(&recorder);
    if (recorder.recording != 0)
        return 1;

    return 0;
}
