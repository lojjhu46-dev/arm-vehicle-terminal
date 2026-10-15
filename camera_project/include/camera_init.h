
#ifndef CAMERA_INIT_H
#define CAMERA_INIT_H
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <asm/types.h>          /* for videodev2.h */
#include <string.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include "jpeglib.h"

//最新改动
#define CLEAR(x) memset(&(x), 0, sizeof(x))
//最新改动

//摄像头采集图像的宽和高
#define  CAMERA_FRAME_W  640
#define  CAMERA_FRAME_H  480

//最新改动
extern int fd;
extern struct buffer *buffers;
extern unsigned int n_buffers;
extern JSAMPLE *image_buffer;
extern JSAMPLE *pimage_buffer;
//最新改动

//自定义类型，用于记录缓冲区信息
struct buffer {
        void *                  start;      //记录缓冲区地址
        size_t                  length;     //记录缓冲区大小
};

//最新改动
int open_device(void);
void init_device(void);
//最新改动

#endif  
