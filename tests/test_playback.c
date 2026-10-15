#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "playback.h"

static int make_directory(const char *path)
{
    return mkdir(path, 0777) == 0 || errno == EEXIST ? 0 : -1;
}

static int make_file(const char *path)
{
    int fd = open(path, O_CREAT | O_WRONLY, 0666);

    if (fd == -1)
        return -1;
    close(fd);
    return 0;
}

int main(void)
{
    PlaybackFrames frames;

    if (make_directory("/tmp/mainui-playback") != 0 ||
        make_directory("/tmp/mainui-playback/2026-07-24_19-00-00") != 0 ||
        make_directory("/tmp/mainui-playback/2026-07-24_20-00-00") != 0 ||
        make_file("/tmp/mainui-playback/2026-07-24_19-00-00/frame0.jpg") != 0 ||
        make_file("/tmp/mainui-playback/2026-07-24_20-00-00/frame10.jpg") != 0 ||
        make_file("/tmp/mainui-playback/2026-07-24_20-00-00/frame2.jpg") != 0)
        return 1;

    if (playback_load_latest(&frames, "/tmp/mainui-playback") != 0)
        return 1;
    if (strcmp(frames.session_dir, "/tmp/mainui-playback/2026-07-24_20-00-00") != 0 ||
        frames.count != 2 ||
        strstr(frames.paths[0], "frame2.jpg") == NULL ||
        strstr(frames.paths[1], "frame10.jpg") == NULL)
        return 1;
    if (playback_load_session(&frames,
                              "/tmp/mainui-playback/2026-07-24_19-00-00") != 0 ||
        frames.count != 1 || strstr(frames.paths[0], "frame0.jpg") == NULL)
        return 1;
    return 0;
}
