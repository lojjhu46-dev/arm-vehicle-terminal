#ifndef RECORDER_H
#define RECORDER_H

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "jpeglib.h"

//最新修改
#define RECORDER_QUEUE_DEPTH 5

// 录像队列中的单帧缓冲状态。
typedef struct {
    JSAMPLE *pixels;
    int state;
} RecorderSlot;

// 异步录像与拍照会话状态。
typedef struct {
    char pathname[256];
    char dirbuffer[256];
    char photo_dir[256];
    int cnt;
    int CNT;
    unsigned int photo_count;
    unsigned int dropped_frames; // 队列满时的丢帧统计。
    int recording;               // 当前录像会话状态。
    pthread_t worker_thread;     // 后台 JPEG 编码线程。
    pthread_mutex_t mutex;       // 保护队列和会话状态。
    pthread_cond_t work_ready;   // 通知后台线程有新任务。
    pthread_cond_t idle;         // 通知测试或退出任务已完成。
    RecorderSlot slots[RECORDER_QUEUE_DEPTH]; // 五块录像帧缓冲。
    unsigned int next_slot;      // 下次优先提交的槽位。
    unsigned int next_worker_slot; // 下次优先编码的槽位。
    JSAMPLE *photo_buffer;       // 独立高优先级拍照缓冲。
    int photo_pending;           // 等待写入的拍照任务。
    int photo_encoding;          // 正在编码的拍照任务。
    int worker_stop;             // 请求后台线程退出。
    int worker_started;          // 后台线程已创建标志。
} Recorder;

int recorder_init(Recorder *recorder);
void recorder_destroy(Recorder *recorder);
int recorder_prepare_session(Recorder *recorder, const char *root_dir, time_t timestamp);
int recorder_start_session(Recorder *recorder, const char *root_dir);
void recorder_stop_session(Recorder *recorder);
int recorder_submit_frame(Recorder *recorder, const JSAMPLE *image_buffer);
int recorder_save_photo(Recorder *recorder, const char *photo_dir,
                        const JSAMPLE *image_buffer);
int recorder_is_recording(Recorder *recorder);
unsigned int recorder_dropped_frames(Recorder *recorder);
void recorder_wait_idle(Recorder *recorder);
const char *recorder_last_path(const Recorder *recorder);
//最新修改

#endif
