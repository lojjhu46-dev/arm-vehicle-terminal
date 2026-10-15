#ifndef IMAGE_CONVERT_H
#define IMAGE_CONVERT_H

#include <stdio.h>

#include "jpeglib.h"

unsigned int yuyv_to_rgb_pix(int Y, int Cb, int Cr, JSAMPLE **pimage_buffer); // 将 YUYV 颜色分量转换成 RGB 像素

//最新修改
// 实时画面复用的 RGB 转换缓冲。
typedef struct {
    JSAMPLE *rgb_buffer; // 一帧 RGB 复用缓冲。
} ImageConverter;

int image_converter_init(ImageConverter *converter); // 分配 RGB 转换缓冲
void image_converter_destroy(ImageConverter *converter); // 释放转换缓冲
JSAMPLE *image_convert_frame(ImageConverter *converter, const char *pbuffer,
                              unsigned int *lcd_mp); // 将摄像头 YUYV 帧转换并拷贝到 LCD
//最新修改

#endif
