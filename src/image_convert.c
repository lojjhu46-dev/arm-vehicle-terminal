#include <stdlib.h>

#include "app_config.h"
#include "image_convert.h"

//转换一个像素点
unsigned int yuyv_to_rgb_pix(int Y, int Cb, int Cr, JSAMPLE **pimage_buffer)
{
	unsigned int pix_point = 0;//存储一个像素点的数据
	// 小改动：使用有符号中间量，保证下限钳制可处理负值。
	int R,G,B;               //用来存储RGB颜色分量

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
    *(*pimage_buffer)++ = R;
    *(*pimage_buffer)++ = G;
    *(*pimage_buffer)++ = B;

	pix_point = (R<<16)|(G<<8)|B;//把RGB合成一个像素点
	return pix_point;
}

//转换一帧   YUYV  YUYVYUYVYUYV -- Y0U0V0 + Y1U0V0
JSAMPLE *image_convert_frame(const char *pbuffer, unsigned int *lcd_mp)
{
    //申请帧缓冲区，用于存储转换好的一帧图像RGB像素点
    JSAMPLE *image_buffer = calloc(1, CAMERA_FRAME_W * CAMERA_FRAME_H * 3);
    JSAMPLE *pimage_buffer = image_buffer;
    unsigned int lcdbuf[CAMERA_FRAME_W * CAMERA_FRAME_H] = {0};
    int i,j;

    if (image_buffer == NULL)
        return NULL;

    //用于记录转换完成的ARGB像素点
    for(i = 0, j = 0; j < CAMERA_FRAME_W * CAMERA_FRAME_H; i += 4, j += 2)
    {
        //                                 Y0          U0        V0
        lcdbuf[j] = yuyv_to_rgb_pix(pbuffer[i], pbuffer[i + 1], pbuffer[i + 3], &pimage_buffer);

        //                                 Y1          U0        V0
        lcdbuf[j + 1] = yuyv_to_rgb_pix(pbuffer[i + 2], pbuffer[i + 1], pbuffer[i + 3], &pimage_buffer);
    }

    //把转换之后的像素点刷新到LCD
    for (size_t y = 0; y < CAMERA_FRAME_H; y++)
    {
        for (size_t x = 80; x < CAMERA_FRAME_W + 80; x++)
        {
           // 小改动：按显示偏移换算源像素索引，避免读取 lcdbuf 越界。
           lcd_mp[y * LCD_WIDTH + x] = lcdbuf[y * CAMERA_FRAME_W + x - 80];
        }
    }
    return image_buffer;
}
