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

void recorder_save_frame(Recorder *recorder, int touch_x, const JSAMPLE *image_buffer)
{
    //启动录像功能
    //需要并发编程优化
    if (touch_x > 720)
    {
        //获取时间戳
        time_t pretime = time(NULL);
        struct tm *time_p = localtime(&pretime);
        //拼接目录完整路径
        sprintf(recorder->dirbuffer, "%d-%.2d-%.2d %.2d:%.2d:%.2d\r\n", time_p->tm_year + 1900,
                time_p->tm_mon + 1, time_p->tm_mday, time_p->tm_hour,
                time_p->tm_min, time_p->tm_sec);
        //创建时间戳目录
        mkdir(recorder->dirbuffer, 0777);
    }

    // 小改动：原 pathname != "0" 恒为真，这里显式保留其每帧保存的实际行为。
    //对帧像素进行jpg编码并保存帧图片到指定路径
    // 小改动：限制路径写入长度，避免异常目录名越过 pathname 缓冲区。
    snprintf(recorder->pathname, sizeof(recorder->pathname),
         "/root/workspace/%.218s/frame%d.jpg",
         recorder->dirbuffer, recorder->cnt++);
    //写入帧图片
    write_JPEG_file(recorder->pathname, 75, image_buffer);
    if (recorder->cnt == JPEG_NUM)
        recorder->cnt = 0;
    if (recorder->CNT == 1799)
    {
        memset(recorder->pathname, 0, sizeof(recorder->pathname));
        recorder->CNT = 0;
    }
    recorder->CNT++;
}

const char *recorder_last_path(const Recorder *recorder)
{
    return recorder->pathname;
}
