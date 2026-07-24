#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "recorder.h"

//最新改动
// 清零录像会话状态和帧编号。
void recorder_init(Recorder *recorder)
{
    memset(recorder, 0, sizeof(*recorder));
}
//最新改动

//最新改动
// 根据指定时间生成安全的录像会话目录路径，但不创建目录。
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
//最新改动

//最新改动
// 首次开始录像时创建时间戳目录并重置帧编号。
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
//最新改动

//最新改动
// 结束当前录像会话，使后续录像重新创建时间戳目录。
void recorder_stop_session(Recorder *recorder)
{
    recorder->recording = 0;
}
//最新改动

//最新改动
// 将 RGB 帧编码为当前录像会话目录中的 JPEG 文件。
int recorder_save_frame(Recorder *recorder, const JSAMPLE *image_buffer)
{
    if (!recorder->recording)
        return 0;

    //对帧像素进行jpg编码并保存帧图片到指定路径
    // 小改动：限制路径写入长度，避免异常目录名越过 pathname 缓冲区。
    snprintf(recorder->pathname, sizeof(recorder->pathname),
             "%.234s/frame%d.jpg", recorder->dirbuffer, recorder->cnt++);
    //写入帧图片
    write_JPEG_file(recorder->pathname, 75, image_buffer);
    if (recorder->cnt == JPEG_NUM)
        recorder->cnt = 0;
    recorder->CNT++;
    return 0;
}
//最新改动

// 将当前 RGB 帧保存为独立照片，不改变录像会话状态。
int recorder_save_photo(Recorder *recorder, const char *photo_dir,
                        const JSAMPLE *image_buffer)
{
    struct tm local_time;
    char timestamp[32];
    time_t now = time(NULL);

    if (localtime_r(&now, &local_time) == NULL)
        return -1;
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &local_time) == 0)
        return -1;
    if (mkdir(photo_dir, 0777) == -1 && errno != EEXIST)
        return -1;
    if (snprintf(recorder->pathname, sizeof(recorder->pathname),
                 "%.210s/photo_%s_%03u.jpg", photo_dir, timestamp,
                 recorder->photo_count++) >= (int)sizeof(recorder->pathname))
        return -1;

    write_JPEG_file(recorder->pathname, 90, image_buffer);
    return 0;
}

//最新改动
// 返回最近成功保存的 JPEG 路径，供回放功能读取。
const char *recorder_last_path(const Recorder *recorder)
{
    return recorder->pathname;
}
//最新改动
