#ifndef TOUCH_H
#define TOUCH_H

#include <pthread.h>

typedef struct {
    int fd;                    // 触摸设备文件描述符
    int touchsrceen_x;         // 转换后的 LCD X 坐标
    int touchsrceen_y;         // 转换后的 LCD Y 坐标
    int touch_active;          // 当前是否处于按下状态
    pthread_mutex_t mutex;     // 保护坐标和状态的互斥锁
} Touch;

int touch_open(Touch *touch);                               // 打开触摸设备
int touch_read(Touch *touch);                               // 读取原始触摸事件
int touch_read_tap(Touch *touch, int *x, int *y);           // 读取一次完整点击
int touch_read_pointer(Touch *touch, int *x, int *y, int *pressed); // 读取指针状态
int touch_poll_pointer(Touch *touch, int *x, int *y, int *pressed); // 非阻塞读取待处理事件
void touch_get_position(Touch *touch, int *x, int *y);      // 获取最近触摸坐标
void touch_reset_position(Touch *touch);                    // 重置触摸坐标状态
void touch_close(Touch *touch);                             // 关闭触摸设备

#endif
