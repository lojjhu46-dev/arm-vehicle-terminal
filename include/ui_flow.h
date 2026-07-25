#ifndef UI_FLOW_H
#define UI_FLOW_H

#include "camera.h"
#include "lcd.h"
#include "recorder.h"
#include "touch.h"
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app_config.h"
#include "image_convert.h"
#include "jpeg_codec.h"
#include "live_controls.h"
#include "playback.h"
//最新改动
#include "overlay.h"
#include "record_indicator.h"
//最新改动
// 界面线程共享状态。
// state_mutex 保护状态变量；lcd_sem 串行化多个线程对 LCD framebuffer 的写入。
typedef struct {
    sem_t lcd_sem;                 // LCD 二值信号量：1 可写，0 正被其他线程写入。
    pthread_mutex_t state_mutex;   // 保护 live_mode、recording 等共享状态。
    int live_mode;                 // 实时影像状态：1 表示摄像头模式，0 表示主界面。
    int recording;                 // 录像状态：1 表示正在保存帧，0 表示未录像。
    int parking_guides_enabled;    // 倒车辅助线状态：1 表示显示，0 表示隐藏。
    unsigned int pending_actions;  // 摄像头线程待执行的录像切换和拍照请求。
    int playback_mode;             // 1 表示正在播放历史录像。
    int touch_running;             // 触摸线程运行标志：1 继续读取，0 请求退出。
} UiState;

// 摄像头线程的运行上下文，集中传递线程需要访问的模块对象。
typedef struct {
    Lcd *lcd;                      // LCD 设备及其 mmap framebuffer 地址。
    Touch *touch;                  // 触摸设备和最近一次触摸坐标。
    Camera *camera;                // V4L2 摄像头设备、缓冲区及当前帧信息。
    Recorder *recorder;            // 录像会话目录、帧编号和录像状态。
    LiveControls *controls;        // 实时画面侧边控制栏图标。
    RecordIndicator *indicator;    // 录像红点和 REC 图标资源。
    UiState *state;                // 与其他线程共享的界面状态和同步对象。
} CameraThreadContext;

void show_startup_animation(Lcd *lcd);
int ui_run(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder);

#endif
