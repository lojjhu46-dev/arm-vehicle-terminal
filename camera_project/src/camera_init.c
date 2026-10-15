#include "camera_init.h"

//最新改动
char *dev_name = "/dev/video7";
int fd = -1;
struct buffer *buffers = NULL;
unsigned int n_buffers = 0;
JSAMPLE *image_buffer = NULL;
JSAMPLE *pimage_buffer = NULL;
//最新改动

//打开设备
int open_device(void)
{
    //打开摄像头设备
    fd = open (dev_name, O_RDWR, 0);

    //错误处理
    if (-1 == fd) 
    {
        printf("open /dev/video7 error\r\n");
        return -1; 
    }
    //最新改动
    return 0;
    //最新改动
}


//初始化摄像头
void init_device(void)
{
    //初始化采集格式  
    struct v4l2_format fmt;
	
    //清空结构体
    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = CAMERA_FRAME_W; 
    fmt.fmt.pix.height      = CAMERA_FRAME_H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    ioctl(fd, VIDIOC_S_FMT, &fmt);     

    //申请缓冲区
    struct v4l2_requestbuffers req;

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    ioctl (fd, VIDIOC_REQBUFS, &req);

    //申请4块结构体大小的堆内存，用于记录待申请的缓冲区信息
    buffers = calloc (req.count, sizeof (*buffers));

    //循环申请4块用于存储图像像素的缓冲区，并记录每块缓冲区的信息
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        ioctl (fd, VIDIOC_QUERYBUF, &buf);


        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
                                    mmap (
                                            NULL /* start anywhere */,
                                            buf.length,
                                            PROT_READ | PROT_WRITE /* required */,
                                            MAP_SHARED /* recommended */,
                                            fd, buf.m.offset);
    }
    
}

//开始采集
//最新改动
void
start_capturing                 (void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    //缓冲区入队
    for (i = 0; i < n_buffers; ++i) 
    {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        ioctl (fd, VIDIOC_QBUF, &buf);
    }
		
    //发送开始采集的请求码
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl (fd, VIDIOC_STREAMON, &type);
}
//最新改动
