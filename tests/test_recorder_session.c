#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
    struct stat status;
    time_t timestamp = 0;

    setenv("TZ", "UTC", 1);
    tzset();
    if (recorder_init(&recorder) != 0)
        return 1;

    if (recorder_prepare_session(&recorder, "/tmp/mainui-recordings", timestamp) != 0)
        return 1;
    if (strcmp(recorder.dirbuffer, "/tmp/mainui-recordings/1970-01-01_00-00-00") != 0)
        return 1;

    if (mkdir("/tmp/mainui-recordings-parent", 0777) != 0 && errno != EEXIST)
        return 1;
    if (recorder_start_session(&recorder,
                               "/tmp/mainui-recordings-parent/recordings") != 0 ||
        stat("/tmp/mainui-recordings-parent/recordings", &status) != 0 ||
        !S_ISDIR(status.st_mode))
        return 1;
    recorder_stop_session(&recorder);

    recorder.recording = 1;
    recorder_stop_session(&recorder);
    if (recorder.recording != 0)
        return 1;

    recorder_destroy(&recorder);

    return 0;
}
