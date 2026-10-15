#include <stdlib.h>

#include "app_config.h"
#include "image_convert.h"

// 将 YUYV 像素转换为 RGB 像素，并写入转换缓冲。
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

//最新修改
// 申请一次 RGB 缓冲，避免逐帧分配内存。
int image_converter_init(ImageConverter *converter)
{
    converter->rgb_buffer = malloc(CAMERA_FRAME_W * CAMERA_FRAME_H * 3);
    return converter->rgb_buffer == NULL ? -1 : 0;
}

// 释放实时画面转换缓冲。
void image_converter_destroy(ImageConverter *converter)
{
    free(converter->rgb_buffer);
    converter->rgb_buffer = NULL;
}

// 转换一帧并且只写入中间摄像头画面，保留两侧静态控制栏。
JSAMPLE *image_convert_frame(ImageConverter *converter, const char *pbuffer,
                              unsigned int *lcd_mp)
{
    JSAMPLE *image_buffer = converter->rgb_buffer;
    JSAMPLE *pimage_buffer = image_buffer;
    size_t input_index = 0;

    if (image_buffer == NULL)
        return NULL;

    for (size_t y = 0; y < CAMERA_FRAME_H; ++y)
    {
        for (size_t x = 0; x < CAMERA_FRAME_W; x += 2)
        {
            unsigned int first_pixel;
            unsigned int second_pixel;

            first_pixel = yuyv_to_rgb_pix((unsigned char)pbuffer[input_index],
                                           (unsigned char)pbuffer[input_index + 1],
                                           (unsigned char)pbuffer[input_index + 3],
                                           &pimage_buffer);
            second_pixel = yuyv_to_rgb_pix((unsigned char)pbuffer[input_index + 2],
                                            (unsigned char)pbuffer[input_index + 1],
                                            (unsigned char)pbuffer[input_index + 3],
                                            &pimage_buffer);
            lcd_mp[y * LCD_WIDTH + CAMERA_DISPLAY_X + x] = first_pixel;
            lcd_mp[y * LCD_WIDTH + CAMERA_DISPLAY_X + x + 1] = second_pixel;
            input_index += 4;
        }
    }
    return image_buffer;
}
//最新修改
