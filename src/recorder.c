#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "recorder.h"

void recorder_init(Recorder *recorder)
{
    memset(recorder, 0, sizeof(*recorder));
}

int recorder_prepare_session(Recorder *recorder, const char *root_dir, time_t timestamp)
{
    struct tm local_time;
    char timestamp_dir[32];

    if (localtime_r(&timestamp, &local_time) == NULL)
        return -1;
    if (strftime(timestamp_dir, sizeof(timestamp_dir), "%Y-%m-%d_%H-%M-%S", &local_time) == 0)
        return -1;

    // 小改动：目录名使用文件系统安全的时间格式，不再包含冒号或换行符。
    if (snprintf(recorder->dirbuffer, sizeof(recorder->dirbuffer), "%.235s/%s",
                 root_dir, timestamp_dir) >= (int)sizeof(recorder->dirbuffer))
        return -1;

    return 0;
}

int recorder_start_session(Recorder *recorder, const char *root_dir)
{
    if (recorder->recording)
        return 0;
    if (recorder_prepare_session(recorder, root_dir, time(NULL)) == -1)
        return -1;

    //创建时间戳目录
    if (mkdir(recorder->dirbuffer, 0777) == -1 && errno != EEXIST)
        return -1;

    recorder->cnt = 0;
    recorder->CNT = 0;
    recorder->recording = 1;
    return 0;
}

void recorder_stop_session(Recorder *recorder)
{
    recorder->recording = 0;
}

void recorder_save_frame(Recorder *recorder, int touch_x, const JSAMPLE *image_buffer)
{
    //启动录像功能
    //需要并发编程优化
    if (touch_x > 720 && !recorder->recording)
    {
        if (recorder_start_session(recorder, "/root/workspace") == -1)
            return;
    }

    if (!recorder->recording)
        return;

    //对帧像素进行jpg编码并保存帧图片到指定路径
    // 小改动：限制路径写入长度，避免异常目录名越过 pathname 缓冲区。
    snprintf(recorder->pathname, sizeof(recorder->pathname),
             "%.234s/frame%d.jpg", recorder->dirbuffer, recorder->cnt++);
    //写入帧图片
    write_JPEG_file(recorder->pathname, 75, image_buffer);
    if (recorder->cnt == JPEG_NUM)
        recorder->cnt = 0;
    recorder->CNT++;
}

const char *recorder_last_path(const Recorder *recorder)
{
    return recorder->pathname;
}
