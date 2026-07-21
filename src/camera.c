#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include "app_config.h"
#include "camera.h"

//清空缓冲区
#define CLEAR(x) memset(&(x), 0, sizeof(x))

//打开设备
int camera_open(Camera *camera, const char *device_name)
{
    //打开摄像头设备
    camera->fd = open(device_name, O_RDWR, 0);

    //错误处理
    if (-1 == camera->fd)
    {
        return -1;
    }
    return 0;
}

//初始化摄像头
int camera_init(Camera *camera)
{
    //初始化采集格式
    struct v4l2_format fmt;

    //清空结构体
    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = CAMERA_FRAME_W;
    fmt.fmt.pix.height = CAMERA_FRAME_H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    if (ioctl(camera->fd, VIDIOC_S_FMT, &fmt) == -1)
        return -1;

    //申请缓冲区
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1)
        return -1;

    //申请4块结构体大小的堆内存，用于记录待申请的缓冲区信息
    camera->buffers = calloc(req.count, sizeof(*camera->buffers));
    if (camera->buffers == NULL)
        return -1;

    //循环申请4块用于存储图像像素的缓冲区，并记录每块缓冲区的信息
    for (camera->n_buffers = 0; camera->n_buffers < req.count; ++camera->n_buffers)
    {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = camera->n_buffers;

        if (ioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1)
            return -1;

        camera->buffers[camera->n_buffers].length = buf.length;
        camera->buffers[camera->n_buffers].start = mmap(
            NULL /* start anywhere */,
            buf.length,
            PROT_READ | PROT_WRITE /* required */,
            MAP_SHARED /* recommended */,
            camera->fd, buf.m.offset);
        if (camera->buffers[camera->n_buffers].start == MAP_FAILED)
            return -1;
    }

    return 0;
}

//开始采集
int camera_start(Camera *camera)
{
    unsigned int i;
    enum v4l2_buf_type type;

    //缓冲区入队
    for (i = 0; i < camera->n_buffers; ++i)
    {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(camera->fd, VIDIOC_QBUF, &buf) == -1)
            return -1;
    }

    //发送开始采集的请求码
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return ioctl(camera->fd, VIDIOC_STREAMON, &type);
}

int camera_wait_frame(Camera *camera)
{
    //定义了一个文件描述符的集合
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);         //清空集合
    FD_SET(camera->fd, &fds);      //添加句柄

    /* Timeout. */
    //设置超时时间
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    return select(camera->fd + 1, &fds, NULL, NULL, &tv);
}

void *camera_dequeue(Camera *camera)
{
    CLEAR(camera->active_buffer);
    camera->active_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    camera->active_buffer.memory = V4L2_MEMORY_MMAP;

    //缓冲区出队
    if (ioctl(camera->fd, VIDIOC_DQBUF, &camera->active_buffer) == -1)
        return NULL;

    // 小改动：使用驱动返回的索引，避免访问与出队帧不对应的缓冲区。
    return camera->buffers[camera->active_buffer.index].start;
}

int camera_requeue(Camera *camera)
{
    //缓冲区入队
    return ioctl(camera->fd, VIDIOC_QBUF, &camera->active_buffer);
}

void camera_close(Camera *camera)
{
    for (unsigned int i = 0; i < camera->n_buffers; ++i)
    {
        if (camera->buffers[i].start != NULL && camera->buffers[i].start != MAP_FAILED)
            munmap(camera->buffers[i].start, camera->buffers[i].length);
    }
    free(camera->buffers);
    if (camera->fd != -1)
        close(camera->fd);
    //最新改动
    /* 关闭后重置状态，允许退出流程再次安全调用 camera_close()。 */
    camera->buffers = NULL;
    camera->n_buffers = 0;
    camera->fd = -1;
    //最新改动
}
