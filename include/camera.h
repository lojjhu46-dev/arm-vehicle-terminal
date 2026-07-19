#ifndef CAMERA_H
#define CAMERA_H

#include <stddef.h>
#include <linux/videodev2.h>

typedef struct {
    void *start;
    size_t length;
} CameraBuffer;

typedef struct {
    int fd;
    CameraBuffer *buffers;
    unsigned int n_buffers;
    struct v4l2_buffer active_buffer;
} Camera;

int camera_open(Camera *camera, const char *device_name);
int camera_init(Camera *camera);
int camera_start(Camera *camera);
int camera_wait_frame(Camera *camera);
void *camera_dequeue(Camera *camera);
int camera_requeue(Camera *camera);
void camera_close(Camera *camera);

#endif
