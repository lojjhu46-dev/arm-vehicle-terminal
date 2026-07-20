#ifndef TOUCH_H
#define TOUCH_H

#include <pthread.h>

typedef struct {
    int fd;
    int touchsrceen_x;
    int touchsrceen_y;
    pthread_mutex_t mutex;
} Touch;

int touch_open(Touch *touch);
int touch_read(Touch *touch);
void touch_get_position(Touch *touch, int *x, int *y);
void touch_reset_position(Touch *touch);
void touch_close(Touch *touch);

#endif
