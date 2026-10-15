#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "app_config.h"
#include "touch.h"

//最新改动
// 打开触摸设备并初始化保护共享坐标的互斥锁。
int touch_open(Touch *touch)
{
    if (pthread_mutex_init(&touch->mutex, NULL) != 0)
        return -1;

    touch->fd = open(TOUCH_PATH, O_RDWR | O_NONBLOCK);
    if (touch->fd == -1)
    {
        perror("open touch_fd error");
        pthread_mutex_destroy(&touch->mutex);
        return -1;
    }
    touch->touchsrceen_x = 0;
    touch->touchsrceen_y = 0;
    touch->touch_active = 0;
    return 0;
}
//最新改动

// 处理单个 input_event；转换坐标并在手指抬起时输出 tap 位置。
static int touch_process_event(Touch *touch, const struct input_event *event,
                               int *tap_x, int *tap_y)
{
    int tapped = 0;

    pthread_mutex_lock(&touch->mutex);
    if (event->type == EV_ABS)
    {
        if (event->code == ABS_X)
            touch->touchsrceen_x = event->value * LCD_WIDTH / 1024;
        else if (event->code == ABS_Y)
            touch->touchsrceen_y = event->value * LCD_HEIGHT / 600;
    }
    else if (event->type == EV_KEY && event->code == BTN_TOUCH)
    {
        if (event->value != 0)
            touch->touch_active = 1;
        else if (touch->touch_active)
        {
            touch->touch_active = 0;
            if (tap_x != NULL)
                *tap_x = touch->touchsrceen_x;
            if (tap_y != NULL)
                *tap_y = touch->touchsrceen_y;
            tapped = 1;
        }
    }
    pthread_mutex_unlock(&touch->mutex);
    return tapped;
}

// 读取一个输入事件，并将触摸原始坐标转换为 LCD 坐标。
int touch_read(Touch *touch)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    if (read(touch->fd, &event, sizeof(event)) != sizeof(event))
        return -1;
    return touch_process_event(touch, &event, NULL, NULL);
}

// 仅在手指抬起时返回一次完整点击及其最终 LCD 坐标。
int touch_read_tap(Touch *touch, int *x, int *y)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    if (read(touch->fd, &event, sizeof(event)) != sizeof(event))
        return -1;
    return touch_process_event(touch, &event, x, y);
}

// 读取一个触摸事件并返回当前坐标和按下状态，供需要拖动输入的界面使用。
int touch_read_pointer(Touch *touch, int *x, int *y, int *pressed)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    if (read(touch->fd, &event, sizeof(event)) != sizeof(event))
        return -1;
    (void)touch_process_event(touch, &event, NULL, NULL);

    pthread_mutex_lock(&touch->mutex);
    if (x != NULL)
        *x = touch->touchsrceen_x;
    if (y != NULL)
        *y = touch->touchsrceen_y;
    if (pressed != NULL)
        *pressed = touch->touch_active;
    pthread_mutex_unlock(&touch->mutex);
    return 0;
}
//最新改动

// 非阻塞处理一个待处理事件；保留按下/抬起边沿供 LVGL 分两轮消费。
int touch_poll_pointer(Touch *touch, int *x, int *y, int *pressed)
{
    struct input_event event;
    ssize_t count;

    for (;;)
    {
        count = read(touch->fd, &event, sizeof(event));

        if (count == (ssize_t)sizeof(event))
        {
            (void)touch_process_event(touch, &event, NULL, NULL);
            break;
        }
        if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0;
        if (count == -1 && errno == EINTR)
            continue;
        if (count == 0)
            return 0;
        return -1;
    }
    pthread_mutex_lock(&touch->mutex);
    if (x != NULL)
        *x = touch->touchsrceen_x;
    if (y != NULL)
        *y = touch->touchsrceen_y;
    if (pressed != NULL)
        *pressed = touch->touch_active;
    pthread_mutex_unlock(&touch->mutex);
    return 1;
}

//最新改动
// 在互斥锁保护下返回最近一次触摸坐标。
void touch_get_position(Touch *touch, int *x, int *y)
{
    pthread_mutex_lock(&touch->mutex);
    *x = touch->touchsrceen_x;
    *y = touch->touchsrceen_y;
    pthread_mutex_unlock(&touch->mutex);
}
//最新改动

//最新改动
// 清除旧坐标，防止同一次点击被下一界面重复处理。
void touch_reset_position(Touch *touch)
{
    pthread_mutex_lock(&touch->mutex);
    touch->touchsrceen_x = -1;
    touch->touchsrceen_y = -1;
    touch->touch_active = 0;
    pthread_mutex_unlock(&touch->mutex);
}
//最新改动

//最新改动
// 关闭触摸设备并销毁坐标互斥锁。
void touch_close(Touch *touch)
{
    if (touch->fd != -1)
        close(touch->fd);
    pthread_mutex_destroy(&touch->mutex);
}
//最新改动
