#include <time.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/mman.h>    
#include<stdlib.h>
#include<unistd.h> 
#include <dirent.h>
#include <string.h>
#include "jpeglib.h"
#include <linux/input.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <setjmp.h>
#define TOUCH_PATH "/dev/input/event0"
#define LCD_PATH "/dev/fb0"
#define LCD_MEM  800*480*4
#define JPEG_NUM 200
//摄像头采集图像的宽和高
#define  CAMERA_FRAME_W  640
#define  CAMERA_FRAME_H  480

static char *           dev_name        = "/dev/video7";  //摄像头的设备节点路径
static int              fd              = -1;    //摄像头驱动的句柄
struct buffer *         buffers         = NULL;  //用于记录缓存信息
static unsigned int     n_buffers       = 0;     
//临时存储帧图片绝对路径的缓冲区
char pathname[256]={0};

//帧图片编号
static int cnt = 0;
static int CNT = 0;
//目录缓冲区
char dirbuffer[256]={0};

extern JSAMPLE * image_buffer;	/* Points to large array of R,G,B-order data */
extern int image_height;	/* Number of rows in image */
extern int image_width;		/* Number of columns in image */

//用于存储转换好的RGB
JSAMPLE * image_buffer   = NULL;
JSAMPLE * pimage_buffer  = NULL;
int lcd_fd;
//清空缓冲区
#define CLEAR(x) memset (&(x), 0, sizeof (x))

//自定义类型，用于记录缓冲区信息
struct buffer {
        void *                  start;      //记录缓冲区地址
        size_t                  length;     //记录缓冲区大小
};


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

int Parking_Assist_Lines_Drawing(int *lcd_mp)
{
    int y_bottom = 470;
    int y_top = 170;

    // 调整这两个值改变斜率和延伸感
    int left_bottom = 30;   // 左线底部位置
    int left_top = 280;     // 左线上端位置，越大越往中间收，延伸感越强

    for(int y = y_bottom; y >= y_top; y--)
    {
        int color = 0x00FF00; // 默认绿色

        if(y > 380 && y < 480)
            color = 0xFF0000;
        else if(y >= 280 && y <= 380)
            color = 0xFFFF00;

        // 虚线间隔
        if((y > 435 && y < 450) ||
           (y > 405 && y < 420) ||
           (y > 300 && y < 315) ||
           (y > 345 && y < 360) ||
           (y > 180 && y < 195) ||
           (y > 220 && y < 235))
        {
            continue;
        }

        // 确定线条宽度
        int width = 5;

        if((y > 420 && y < 430) || (y > 320 && y < 330))
            width = 40;
        else if(y >= 200 && y < 210)
            width = 20;
        else if(y <= 180)
            width = 170;

        // 根据 y 计算左辅助线当前 x 坐标
        int left_x = left_bottom +
                     (y_bottom - y) * (left_top - left_bottom) /
                     (y_bottom - y_top);

        // 右辅助线对称
        int right_x = 799 - left_x;

        // 画左辅助线，向右加粗
        for(int i = 0; i < width; i++)
        {
            int x = left_x + i;

            if(x >= 0 && x < 800 && y >= 0 && y < 480)
                lcd_mp[y * 800 + x] = color;
        }

        // 画右辅助线，向左加粗
        for(int i = 0; i < width; i++)
        {
            int x = right_x - i;

            if(x >= 0 && x < 800 && y >= 0 && y < 480)
                lcd_mp[y * 800 + x] = color;
        }
    }

    return 0;
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

    //启动录像功能
    //需要并发编程优化
    if (lcd_touch_info_p->touchsrceen_x>720)
    {
        //获取时间戳
        time_t pretime =time(NULL);
        struct tm *time_p=localtime(&pretime);
         //拼接目录完整路径
            sprintf(dirbuffer,"%d-%.2d-%.2d %.2d:%.2d:%.2d\r\n",time_p->tm_year+1900,
            time_p->tm_mon+1,time_p->tm_mday,time_p->tm_hour,
            time_p->tm_min,time_p->tm_sec);
            //创建时间戳目录
            mkdir(dirbuffer,0777);
    }
    if (pathname!="0")
    {
        //对帧像素进行jpg编码并保存帧图片到指定路径
        sprintf(pathname,"/root/workspace/%s/frame%d.jpg",dirbuffer,cnt++);
        //写入帧图片
        write_JPEG_file (pathname,75);
        if(cnt == 200) cnt =0;
        if (CNT==1799)
        {
            memset(pathname,0,sizeof(pathname));
            CNT = 0;
        }
        CNT++;
    }
    //用于记录转换完成的ARGB像素点
    unsigned int lcdbuf[CAMERA_FRAME_W*CAMERA_FRAME_H] = {0};

    for(i=0,j=0;j<CAMERA_FRAME_W*CAMERA_FRAME_H;i+=4,j+=2)
    {
        //                                 Y0          U0        V0
        lcdbuf[j]   = yuyv_to_rgb_pix(pbuffer[i],pbuffer[i+1],pbuffer[i+3]);   

        //                                 Y1          U0        V0 
        lcdbuf[j+1] = yuyv_to_rgb_pix(pbuffer[i+2],pbuffer[i+1],pbuffer[i+3]);
    }

    //把转换之后的像素点刷新到LCD
    for (size_t y = 0; y < CAMERA_FRAME_H; y++)
    {
        for (size_t x = 80; x < CAMERA_FRAME_W+80; x++)
        {  
           lcd_mp[y*800+x] = lcdbuf[y*CAMERA_FRAME_W+x];
        }    
    }
    Parking_Assist_Lines_Drawing(lcd_mp);
}

//打开设备
int open_device(void)
{
    //打开摄像头设备
    fd = open (dev_name, O_RDWR, 0);

    //错误处理
    if (-1 == fd) 
    {
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


struct Lcd_Touch_str
{
    int touchsrceen_x;
    int touchsrceen_y;
}* lcd_touch_info_p;
 
int lcd_touch(int lcd_fd)
{
    int touch_fd=open(TOUCH_PATH,O_RDWR);
    if(touch_fd==-1) 
    {
        perror("open touch_fd error");
        return -1;
    }
    struct input_event touchsrceen_event;
    memset(&touchsrceen_event,0,sizeof(struct input_event));
    {
        read(touch_fd,&touchsrceen_event,sizeof(struct input_event));
        if (touchsrceen_event.type==EV_ABS)
        {
            if (touchsrceen_event.code==ABS_X)
            {
                lcd_touch_info_p->touchsrceen_x=touchsrceen_event.value*800/1024;
            }
            else if (touchsrceen_event.code==ABS_Y)
            {
                lcd_touch_info_p->touchsrceen_y=touchsrceen_event.value*480/600;
            }
        }
    }
    return lcd_touch_info_p;
}


//处理图像
static void
mainloop                        (unsigned int *lcd_mp)
{
	struct v4l2_buffer buf;
	unsigned int i;
    int x,y;
    //跳转到指定目录下
    chdir("/root/workspace/");
    for (;;) 
    {
        lcd_touch(lcd_fd);
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
        if (lcd_touch_info_p->touchsrceen_x<80)
        {
            //点击指定区域触发回放功能
                for (int i = 0; i < 200; i++)
                {
                    read_JPEG_file(pathname, lcd_mp);
                    usleep(30 * 1000);
                }

        } 
        
    }
       
}


int read_JPEG_file (char * filename,int *lcd_mp)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
	struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE * infile;		/* source file */
  int row_stride;		/* physical row width in output buffer */
  unsigned char *buffer=NULL;
  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    return 0;
  }

  /* Step 1: allocate and initialize JPEG decompression object */
  cinfo.err = jpeg_std_error(&jerr);
  /* We set up the normal JPEG error routines, then override error_exit. */
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */

  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.txt for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = calloc(row_stride,1);
  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    jpeg_read_scanlines(&cinfo, &buffer, 1);
    for (size_t i = 0; i < cinfo.image_width; i++)
    {
      lcd_mp[(cinfo.output_scanline-1)*800+i]=buffer[i*3]<<16|buffer[i*3+1]<<8|buffer[i*3+2];
    }
    
  }

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
  fclose(infile);
  free(buffer);
  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */
  return 0;
}


int main()
{
  lcd_fd=open(LCD_PATH,O_RDWR);
  if (lcd_fd==-1)
  {
    perror("open fb0 fail");
    return -1;
  }

  int *lcd_mp=mmap(NULL,LCD_MEM,PROT_WRITE|PROT_READ,MAP_SHARED,lcd_fd,0);
  if (lcd_mp==(void*)-1)
  {
    perror("mmap fb0 fail");
    return -1;
  }

  char *frames[] = {
    "./logo/frame-0.jpeg", "./logo/frame-1.jpeg",
    "./logo/frame-2.jpeg", "./logo/frame-3.jpeg",
    "./logo/frame-4.jpeg", "./logo/main_UI.jpeg"
    };

  for (int round = 0; round < 5; round++) 
  {
      for (int i = 0; i < 4; i++)
      {
          read_JPEG_file(frames[i], lcd_mp);
          usleep(100 * 1000);
      }
  }
  read_JPEG_file(frames[4],lcd_mp);
  usleep(400 * 1000);
  read_JPEG_file(frames[5],lcd_mp);
  int x,y;
  for(;;)
  {
    lcd_touch(lcd_fd);
    x=lcd_touch_info_p->touchsrceen_x;
    y=lcd_touch_info_p->touchsrceen_y;
    if ((lcd_touch_info_p->touchsrceen_x>10&&lcd_touch_info_p->touchsrceen_x<70)&&(lcd_touch_info_p->touchsrceen_y>70&&lcd_touch_info_p->touchsrceen_y<110))
    {
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
        break;
    }
    else continue;
    
  }
  
    return 0;
}


