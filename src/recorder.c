#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "recorder.h"

//最新修改
#define RECORDER_SLOT_FREE 0
#define RECORDER_SLOT_QUEUED 1
#define RECORDER_SLOT_ENCODING 2
#define RGB_FRAME_BYTES (CAMERA_FRAME_W * CAMERA_FRAME_H * 3)

// 判断是否仍有待写入的录像帧。
static int recorder_has_queued_frame(const Recorder *recorder)
{
    for (unsigned int i = 0; i < RECORDER_QUEUE_DEPTH; ++i)
    {
        if (recorder->slots[i].state == RECORDER_SLOT_QUEUED)
            return 1;
    }
    return 0;
}

// 判断后台线程是否仍在处理任务。
static int recorder_is_busy_locked(const Recorder *recorder)
{
    if (recorder->photo_pending || recorder->photo_encoding ||
        recorder_has_queued_frame(recorder))
        return 1;
    for (unsigned int i = 0; i < RECORDER_QUEUE_DEPTH; ++i)
    {
        if (recorder->slots[i].state == RECORDER_SLOT_ENCODING)
            return 1;
    }
    return 0;
}

// 按当前时间生成照片输出路径。
static int recorder_build_photo_path(Recorder *recorder, char *path, size_t path_size)
{
    struct tm local_time;
    char timestamp[32];
    time_t now = time(NULL);

    if (localtime_r(&now, &local_time) == NULL)
        return -1;
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &local_time) == 0)
        return -1;
    if (snprintf(path, path_size, "%.210s/photo_%s_%03u.jpg", recorder->photo_dir,
                 timestamp, recorder->photo_count++) >= (int)path_size)
        return -1;
    snprintf(recorder->pathname, sizeof(recorder->pathname), "%s", path);
    return 0;
}

// 生成下一张录像帧的输出路径。
static int recorder_build_frame_path(Recorder *recorder, char *path, size_t path_size)
{
    if (snprintf(path, path_size, "%.234s/frame%d.jpg", recorder->dirbuffer,
                 recorder->cnt) >= (int)path_size)
        return -1;
    recorder->cnt = (recorder->cnt + 1) % JPEG_NUM;
    recorder->CNT++;
    snprintf(recorder->pathname, sizeof(recorder->pathname), "%s", path);
    return 0;
}

// 选择最早等待编码的录像缓冲。
static int recorder_take_frame_locked(Recorder *recorder, unsigned int *slot_index,
                                      char *path, size_t path_size)
{
    for (unsigned int offset = 0; offset < RECORDER_QUEUE_DEPTH; ++offset)
    {
        unsigned int index = (recorder->next_worker_slot + offset) % RECORDER_QUEUE_DEPTH;

        if (recorder->slots[index].state == RECORDER_SLOT_QUEUED)
        {
            recorder->slots[index].state = RECORDER_SLOT_ENCODING;
            recorder->next_worker_slot = (index + 1) % RECORDER_QUEUE_DEPTH;
            if (recorder_build_frame_path(recorder, path, path_size) == -1)
            {
                recorder->slots[index].state = RECORDER_SLOT_FREE;
                return -1;
            }
            *slot_index = index;
            return 0;
        }
    }
    return -1;
}

// 后台线程优先保存拍照任务，再顺序编码录像帧。
static void *recorder_worker_main(void *argument)
{
    Recorder *recorder = argument;

    for (;;)
    {
        JSAMPLE *image_buffer;
        unsigned int slot_index = 0;
        char pathname[256];
        int is_photo = 0;

        pthread_mutex_lock(&recorder->mutex);
        while (!recorder->worker_stop && !recorder->photo_pending &&
               !recorder_has_queued_frame(recorder))
            pthread_cond_wait(&recorder->work_ready, &recorder->mutex);
        if (recorder->worker_stop)
        {
            pthread_mutex_unlock(&recorder->mutex);
            break;
        }

        if (recorder->photo_pending)
        {
            recorder->photo_pending = 0;
            recorder->photo_encoding = 1;
            image_buffer = recorder->photo_buffer;
            is_photo = 1;
            if (recorder_build_photo_path(recorder, pathname, sizeof(pathname)) == -1)
            {
                recorder->photo_encoding = 0;
                pthread_cond_broadcast(&recorder->idle);
                pthread_mutex_unlock(&recorder->mutex);
                continue;
            }
        }
        else if (recorder_take_frame_locked(recorder, &slot_index, pathname,
                                             sizeof(pathname)) == 0)
        {
            image_buffer = recorder->slots[slot_index].pixels;
        }
        else
        {
            pthread_mutex_unlock(&recorder->mutex);
            continue;
        }
        pthread_mutex_unlock(&recorder->mutex);

        write_JPEG_file(pathname, is_photo ? 90 : 75, image_buffer);

        pthread_mutex_lock(&recorder->mutex);
        if (is_photo)
            recorder->photo_encoding = 0;
        else
            recorder->slots[slot_index].state = RECORDER_SLOT_FREE;
        pthread_cond_broadcast(&recorder->idle);
        pthread_mutex_unlock(&recorder->mutex);
    }
    return NULL;
}

// 初始化后台编码线程与单独的拍照缓冲。
int recorder_init(Recorder *recorder)
{
    memset(recorder, 0, sizeof(*recorder));
    if (pthread_mutex_init(&recorder->mutex, NULL) != 0)
        return -1;
    if (pthread_cond_init(&recorder->work_ready, NULL) != 0)
    {
        pthread_mutex_destroy(&recorder->mutex);
        return -1;
    }
    if (pthread_cond_init(&recorder->idle, NULL) != 0)
    {
        pthread_cond_destroy(&recorder->work_ready);
        pthread_mutex_destroy(&recorder->mutex);
        return -1;
    }
    recorder->photo_buffer = malloc(RGB_FRAME_BYTES);
    if (recorder->photo_buffer == NULL)
    {
        pthread_cond_destroy(&recorder->idle);
        pthread_cond_destroy(&recorder->work_ready);
        pthread_mutex_destroy(&recorder->mutex);
        return -1;
    }
    if (pthread_create(&recorder->worker_thread, NULL, recorder_worker_main, recorder) != 0)
    {
        free(recorder->photo_buffer);
        pthread_cond_destroy(&recorder->idle);
        pthread_cond_destroy(&recorder->work_ready);
        pthread_mutex_destroy(&recorder->mutex);
        return -1;
    }
    recorder->worker_started = 1;
    return 0;
}

// 停止后台线程并回收全部预申请缓冲。
void recorder_destroy(Recorder *recorder)
{
    if (recorder->worker_started)
    {
        pthread_mutex_lock(&recorder->mutex);
        recorder->worker_stop = 1;
        pthread_cond_signal(&recorder->work_ready);
        pthread_mutex_unlock(&recorder->mutex);
        pthread_join(recorder->worker_thread, NULL);
    }
    for (unsigned int i = 0; i < RECORDER_QUEUE_DEPTH; ++i)
        free(recorder->slots[i].pixels);
    free(recorder->photo_buffer);
    pthread_cond_destroy(&recorder->idle);
    pthread_cond_destroy(&recorder->work_ready);
    pthread_mutex_destroy(&recorder->mutex);
    memset(recorder, 0, sizeof(*recorder));
}

// 根据指定时间生成安全的录像会话目录路径。
int recorder_prepare_session(Recorder *recorder, const char *root_dir, time_t timestamp)
{
    struct tm local_time;
    char timestamp_dir[32];

    if (localtime_r(&timestamp, &local_time) == NULL)
        return -1;
    if (strftime(timestamp_dir, sizeof(timestamp_dir), "%Y-%m-%d_%H-%M-%S", &local_time) == 0)
        return -1;
    if (snprintf(recorder->dirbuffer, sizeof(recorder->dirbuffer), "%.235s/%s",
                 root_dir, timestamp_dir) >= (int)sizeof(recorder->dirbuffer))
        return -1;
    return 0;
}

// 首次录像时预申请五块 RGB 队列缓冲。
int recorder_start_session(Recorder *recorder, const char *root_dir)
{
    pthread_mutex_lock(&recorder->mutex);
    if (recorder->recording)
    {
        pthread_mutex_unlock(&recorder->mutex);
        return 0;
    }
    if (recorder_prepare_session(recorder, root_dir, time(NULL)) == -1 ||
        (mkdir(recorder->dirbuffer, 0777) == -1 && errno != EEXIST))
    {
        pthread_mutex_unlock(&recorder->mutex);
        return -1;
    }
    for (unsigned int i = 0; i < RECORDER_QUEUE_DEPTH; ++i)
    {
        if (recorder->slots[i].pixels == NULL)
            recorder->slots[i].pixels = malloc(RGB_FRAME_BYTES);
        if (recorder->slots[i].pixels == NULL)
        {
            for (unsigned int j = 0; j < RECORDER_QUEUE_DEPTH; ++j)
            {
                free(recorder->slots[j].pixels);
                recorder->slots[j].pixels = NULL;
            }
            pthread_mutex_unlock(&recorder->mutex);
            return -1;
        }
        if (recorder->slots[i].state != RECORDER_SLOT_ENCODING)
            recorder->slots[i].state = RECORDER_SLOT_FREE;
    }
    recorder->cnt = 0;
    recorder->CNT = 0;
    recorder->dropped_frames = 0;
    recorder->next_slot = 0;
    recorder->next_worker_slot = 0;
    recorder->recording = 1;
    pthread_mutex_unlock(&recorder->mutex);
    return 0;
}

// 停止录像并丢弃尚未开始编码的录像帧。
void recorder_stop_session(Recorder *recorder)
{
    pthread_mutex_lock(&recorder->mutex);
    recorder->recording = 0;
    for (unsigned int i = 0; i < RECORDER_QUEUE_DEPTH; ++i)
    {
        if (recorder->slots[i].state == RECORDER_SLOT_QUEUED)
            recorder->slots[i].state = RECORDER_SLOT_FREE;
    }
    pthread_cond_broadcast(&recorder->idle);
    pthread_mutex_unlock(&recorder->mutex);
}

// 非阻塞提交录像帧；队列满时只记录丢帧。
int recorder_submit_frame(Recorder *recorder, const JSAMPLE *image_buffer)
{
    pthread_mutex_lock(&recorder->mutex);
    if (!recorder->recording)
    {
        pthread_mutex_unlock(&recorder->mutex);
        return 0;
    }
    for (unsigned int offset = 0; offset < RECORDER_QUEUE_DEPTH; ++offset)
    {
        unsigned int index = (recorder->next_slot + offset) % RECORDER_QUEUE_DEPTH;

        if (recorder->slots[index].state == RECORDER_SLOT_FREE)
        {
            memcpy(recorder->slots[index].pixels, image_buffer, RGB_FRAME_BYTES);
            recorder->slots[index].state = RECORDER_SLOT_QUEUED;
            recorder->next_slot = (index + 1) % RECORDER_QUEUE_DEPTH;
            pthread_cond_signal(&recorder->work_ready);
            pthread_mutex_unlock(&recorder->mutex);
            return 0;
        }
    }
    recorder->dropped_frames++;
    pthread_mutex_unlock(&recorder->mutex);
    return 1;
}

// 提交优先于录像帧的单张拍照任务。
int recorder_save_photo(Recorder *recorder, const char *photo_dir,
                        const JSAMPLE *image_buffer)
{
    pthread_mutex_lock(&recorder->mutex);
    if (recorder->photo_pending || recorder->photo_encoding ||
        (mkdir(photo_dir, 0777) == -1 && errno != EEXIST) ||
        snprintf(recorder->photo_dir, sizeof(recorder->photo_dir), "%s", photo_dir) >=
            (int)sizeof(recorder->photo_dir))
    {
        pthread_mutex_unlock(&recorder->mutex);
        return -1;
    }
    memcpy(recorder->photo_buffer, image_buffer, RGB_FRAME_BYTES);
    recorder->photo_pending = 1;
    pthread_cond_signal(&recorder->work_ready);
    pthread_mutex_unlock(&recorder->mutex);
    return 0;
}

// 返回当前录像会话状态。
int recorder_is_recording(Recorder *recorder)
{
    int recording;

    pthread_mutex_lock(&recorder->mutex);
    recording = recorder->recording;
    pthread_mutex_unlock(&recorder->mutex);
    return recording;
}

// 返回因队列占满而丢弃的录像帧数。
unsigned int recorder_dropped_frames(Recorder *recorder)
{
    unsigned int dropped_frames;

    pthread_mutex_lock(&recorder->mutex);
    dropped_frames = recorder->dropped_frames;
    pthread_mutex_unlock(&recorder->mutex);
    return dropped_frames;
}

// 等待后台任务完成，供测试和受控退出使用。
void recorder_wait_idle(Recorder *recorder)
{
    pthread_mutex_lock(&recorder->mutex);
    while (recorder_is_busy_locked(recorder))
        pthread_cond_wait(&recorder->idle, &recorder->mutex);
    pthread_mutex_unlock(&recorder->mutex);
}

// 返回最近一次排队的 JPEG 路径。
const char *recorder_last_path(const Recorder *recorder)
{
    return recorder->pathname;
}
//最新修改
