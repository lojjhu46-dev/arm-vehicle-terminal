#ifndef TOUCH_H
#define TOUCH_H

typedef struct {
    int fd;
    int touchsrceen_x;
    int touchsrceen_y;
} Touch;

int touch_open(Touch *touch);
int touch_read(Touch *touch);
void touch_close(Touch *touch);

#endif
