#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "app_config.h"
#include "touch.h"

int touch_open(Touch *touch)
{
    if (pthread_mutex_init(&touch->mutex, NULL) != 0)
        return -1;

    touch->fd = open(TOUCH_PATH, O_RDWR);
    if (touch->fd == -1)
    {
        perror("open touch_fd error");
        pthread_mutex_destroy(&touch->mutex);
        return -1;
    }
    touch->touchsrceen_x = 0;
    touch->touchsrceen_y = 0;
    return 0;
}

int touch_read(Touch *touch)
{
    struct input_event touchsrceen_event;

    memset(&touchsrceen_event, 0, sizeof(struct input_event));
    if (read(touch->fd, &touchsrceen_event, sizeof(struct input_event)) < 0)
        return -1;

    if (touchsrceen_event.type == EV_ABS)
    {
        pthread_mutex_lock(&touch->mutex);
        if (touchsrceen_event.code == ABS_X)
        {
            touch->touchsrceen_x = touchsrceen_event.value * LCD_WIDTH / 1024;
        }
        else if (touchsrceen_event.code == ABS_Y)
        {
            touch->touchsrceen_y = touchsrceen_event.value * LCD_HEIGHT / 600;
        }
        pthread_mutex_unlock(&touch->mutex);
    }
    return 0;
}

void touch_get_position(Touch *touch, int *x, int *y)
{
    pthread_mutex_lock(&touch->mutex);
    *x = touch->touchsrceen_x;
    *y = touch->touchsrceen_y;
    pthread_mutex_unlock(&touch->mutex);
}

void touch_reset_position(Touch *touch)
{
    pthread_mutex_lock(&touch->mutex);
    touch->touchsrceen_x = -1;
    touch->touchsrceen_y = -1;
    pthread_mutex_unlock(&touch->mutex);
}

void touch_close(Touch *touch)
{
    if (touch->fd != -1)
        close(touch->fd);
    pthread_mutex_destroy(&touch->mutex);
}
