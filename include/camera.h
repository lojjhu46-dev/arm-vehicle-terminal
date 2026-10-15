#ifndef CAMERA_H
#define CAMERA_H

#include <stddef.h>
#include <linux/videodev2.h>

typedef struct {
    void *start;   // mmap 映射起始地址
    size_t length; // 缓冲区长度
} CameraBuffer;

typedef struct {
    int fd;                        // 摄像头设备文件描述符
    CameraBuffer *buffers;         // mmap 缓冲数组
    unsigned int n_buffers;        // 缓冲区数量
    struct v4l2_buffer active_buffer; // 当前出队帧信息
} Camera;

int camera_open(Camera *camera, const char *device_name);   // 打开摄像头设备
int camera_init(Camera *camera);                            // 配置格式并申请 mmap 缓冲
int camera_start(Camera *camera);                           // 开始视频流并入队缓冲区
int camera_wait_frame(Camera *camera);                      // 等待一帧可读取
void *camera_dequeue(Camera *camera);                       // 从驱动出队一帧
int camera_requeue(Camera *camera);                         // 重入出队帧缓冲
void camera_close(Camera *camera);                          // 清理摄像头资源

#endif
