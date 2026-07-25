#ifndef IMAGE_CONVERT_H
#define IMAGE_CONVERT_H

#include <stdio.h>

#include "jpeglib.h"

unsigned int yuyv_to_rgb_pix(int Y, int Cb, int Cr, JSAMPLE **pimage_buffer);

//最新修改
// 实时画面复用的 RGB 转换缓冲。
typedef struct {
    JSAMPLE *rgb_buffer; // 一帧 RGB 复用缓冲。
} ImageConverter;

int image_converter_init(ImageConverter *converter);
void image_converter_destroy(ImageConverter *converter);
JSAMPLE *image_convert_frame(ImageConverter *converter, const char *pbuffer,
                              unsigned int *lcd_mp);
//最新修改

#endif
