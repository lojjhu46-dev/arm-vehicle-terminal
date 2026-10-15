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

//LCD设备驱动路径
#define  LCD_PATH   "/dev/fb0"

//LCD内存大小
#define  LCD_MEM    800*480*4

//摄像头采集图像的宽和高
#define  CAMERA_FRAME_W  640
#define  CAMERA_FRAME_H  480

static char *           dev_name        = "/dev/video7";  //摄像头的设备节点路径
static int              fd              = -1;    //摄像头驱动的句柄
struct buffer *         buffers         = NULL;  //用于记录缓存信息
static unsigned int     n_buffers       = 0;     

//用于存储转换好的RGB
JSAMPLE * image_buffer   = NULL;
JSAMPLE * pimage_buffer  = NULL;

//清空缓冲区
#define CLEAR(x) memset (&(x), 0, sizeof (x))

//自定义类型，用于记录缓冲区信息
struct buffer {
        void *                  start;      //记录缓冲区地址
        size_t                  length;     //记录缓冲区大小
};

int open_device(void);
void init_device(void);
static void start_capturing (void);
static void mainloop(unsigned int *lcd_mp);
GLOBAL(void) write_JPEG_file (char * filename, int quality);
int yuyv_to_rgb_pix(int Y,int Cb,int Cr);
void yuv2rgb(char *pbuffer,unsigned int *lcd_mp);

GLOBAL(void)
write_JPEG_file (char * filename, int quality)
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE * outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
  if ((outfile = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = CAMERA_FRAME_W; 	/* image width and height, in pixels */
  cinfo.image_height = CAMERA_FRAME_H;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = CAMERA_FRAME_W * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) 
  {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  fclose(outfile);

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  /* And we're done! */
}


//转换一个像素点
int yuyv_to_rgb_pix(int Y,int Cb,int Cr)
{

	unsigned int pix_point = 0;//存储一个像素点的数据
	unsigned int R,G,B;      //用来存储RGB颜色分量

	//通过V4L2官方帮助文档中的公式进行计算
	R = (255/219)*(Y - 16) + 1.402 * (127/112)*(Cr - 128);
	G = (255/219)*(Y - 16) - 0.344 * (127/112)*(Cb - 128) - 0.714*(127/112)*(Cr - 128);
	B = (255/219)*(Y - 16) + 1.772 * (127/112)*(Cb - 128);

	//防止越界  RGB分别为占用一个字节 0~255，所以需要进行越界处理
	if(R > 255) R = 255;
	if(G > 255) G = 255;
	if(B > 255) B = 255;
	if(R < 0) R = 0;
	if(G < 0) G = 0;
	if(B < 0) B = 0;

    //把转换之后的像素点写入到缓冲区
    *(pimage_buffer++) = R;
    *(pimage_buffer++) = G;
    *(pimage_buffer++) = B;

	pix_point = (R<<16)|(G<<8)|B;//把RGB合成一个像素点 
	return pix_point;
}

//转换一帧   YUYV  YUYVYUYVYUYV -- Y0U0V0 + Y1U0V0
void yuv2rgb(char *pbuffer,unsigned int *lcd_mp)
{
    //申请帧缓冲区，用于存储转换好的一帧图像RGB像素点
    image_buffer = (JSAMPLE *)calloc(1,CAMERA_FRAME_W*CAMERA_FRAME_H*3);
    pimage_buffer = image_buffer;

    int i,j;

    //用于记录转换完成的ARGB像素点
    unsigned int lcdbuf[CAMERA_FRAME_W*CAMERA_FRAME_H] = {0};

    for(i=0,j=0;j<CAMERA_FRAME_W*CAMERA_FRAME_H;i+=4,j+=2)
    {
        //                                 Y0          U0        V0
        lcdbuf[j]   = yuyv_to_rgb_pix(pbuffer[i],pbuffer[i+1],pbuffer[i+3]);   

        //                                 Y1          U0        V0 
        lcdbuf[j+1] = yuyv_to_rgb_pix(pbuffer[i+2],pbuffer[i+1],pbuffer[i+3]);
    }

    //对帧像素进行jpg编码
    static int cnt = 0;
    char framename[128] = {0};
    sprintf(framename,"frame%d.jpg",cnt++);
    write_JPEG_file (framename,75);


    //把转换之后的像素点刷新到LCD
    for (size_t y = 0; y < CAMERA_FRAME_H; y++)
    {
        for (size_t x = 0; x < CAMERA_FRAME_W; x++)
        {
           lcd_mp[y*800+x] = lcdbuf[y*CAMERA_FRAME_W+x];
        }    
    }
    
}


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
static void
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

//处理图像
static void
mainloop                        (unsigned int *lcd_mp)
{
	struct v4l2_buffer buf;
	unsigned int i;

    for (;;) 
    {
        //定义了一个文件描述符的集合
        fd_set fds;             
        struct timeval tv; 
        int r;

        FD_ZERO (&fds);         //清空集合
        FD_SET (fd, &fds);      //添加句柄 

        /* Timeout. */
        //设置超时时间
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        
        select (fd + 1, &fds, NULL, NULL, &tv);
        
        //循环 出队->处理->入队
        for (i = 0; i < 4; ++i) 
        {
            //读取图像帧并进行颜色转码，刷新到LCD
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            //缓冲区出队
            ioctl (fd, VIDIOC_DQBUF, &buf);

            //处理图像帧并显示到LCD
            yuv2rgb(buffers[i].start,lcd_mp);
 
            //缓冲区入队
            ioctl (fd, VIDIOC_QBUF, &buf);
        }
        
    }
       
}






#include "video_phone.h"

//接收线程
void *recv_thread(void *arg)
{
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    char recvbuf[1024];
    for(;;)
    {
        pthread_mutex_lock(&udp_lock);
        memset(recvbuf,0,sizeof(recvbuf));
        int len = recvfrom(
                udp_fd,
                recvbuf,
                sizeof(recvbuf)-1,
                0,
                (struct sockaddr *)&src_addr,
                &addr_len
        );
        pthread_mutex_unlock(&udp_lock);
        if(len > 0)
        {
            recvbuf[len]='\0';

            printf("\n收到消息:%s\n",recvbuf);
            printf("请输入:");
            fflush(stdout);
        }
    }

}

int Udp_Main_thread()
{
    pthread_t tid;
    //创建UDP socket
    
    udp_fd = socket(AF_INET,SOCK_DGRAM,0);
    if(udp_fd < 0)
    {
        perror("socket");
        return -1;
    }

    //绑定本地端口
    
    struct sockaddr_in local_addr;
    memset(&local_addr,0,sizeof(local_addr));

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(udp_fd,(struct sockaddr *)&local_addr,sizeof(local_addr)))
    {
        perror("bind");
        return -1;
    }

    //创建接收线程
    
    pthread_create(
            &tid,
            NULL,
            recv_thread,
            NULL
    );

    //目标地址
    struct sockaddr_in dest_addr;
    memset(&dest_addr,0,sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    //修改为对方IP
    dest_addr.sin_addr.s_addr = inet_addr("192.168.33.52");
    char sendbuf[1024];
    for(;;)
    {
        pthread_mutex_lock(&udp_lock);
        printf("请输入:");
        scanf("%s",sendbuf);
        sendto(
            udp_fd,
            sendbuf,
            strlen(sendbuf),
            0,
            (struct sockaddr *)&dest_addr,
            sizeof(dest_addr)
        );
        pthread_mutex_unlock(&udp_lock);
    }
    close(udp_fd);
    return 0;
}





int main()
{
    //1.打开LCD设备驱动
    int lcd_fd = open(LCD_PATH,O_RDWR);
    //错误处理
    if(-1 == lcd_fd)
    {
        perror("open lcd device error\r\n");
        return -1;
    }

    //2.对LCD设备文件进行内存映射，目的是提高LCD刷新效率
    unsigned int *lcd_mp = mmap(   
                                    NULL,                       //待申请的内存地址，由kernel自动分配
                                    LCD_MEM,                    //待申请的内存大小
                                    PROT_READ|PROT_WRITE,       //权限是可读可写
                                    MAP_SHARED,                 //共享内存，共享给其他进程
                                    lcd_fd,                     //待映射的文件句柄，前提是要open
                                    0                           //地址偏移，不偏移
                              );

    //3.打开摄像头
    open_device();
                              
    //4.初始化摄像头
    init_device();

    //5.开始采集
    start_capturing();

    //6.处理图像
    mainloop(lcd_mp);

    //7.关闭LCD设备文件并解除映射
    close(lcd_fd);
    munmap(lcd_mp,LCD_MEM);

    return 0;
}

    