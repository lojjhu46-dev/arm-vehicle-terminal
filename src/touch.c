#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "app_config.h"
#include "touch.h"

int touch_open(Touch *touch)
{
    touch->fd = open(TOUCH_PATH, O_RDWR);
    if (touch->fd == -1)
    {
        perror("open touch_fd error");
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
        if (touchsrceen_event.code == ABS_X)
        {
            touch->touchsrceen_x = touchsrceen_event.value * LCD_WIDTH / 1024;
        }
        else if (touchsrceen_event.code == ABS_Y)
        {
            touch->touchsrceen_y = touchsrceen_event.value * LCD_HEIGHT / 600;
        }
    }
    return 0;
}

void touch_close(Touch *touch)
{
    if (touch->fd != -1)
        close(touch->fd);
}
