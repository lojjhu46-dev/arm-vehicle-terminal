#ifndef JPEG_CODEC_H
#define JPEG_CODEC_H

#include <stdio.h>

#include "jpeglib.h"

void write_JPEG_file(const char *filename, int quality, const JSAMPLE *image_buffer); // 写入 RGB 数据到 JPEG 文件
int read_JPEG_file(const char *filename, int *lcd_mp); // 将 JPEG 解码到 framebuffer
int read_JPEG_image(const char *filename, int **pixels, int *width, int *height); // 将 JPEG 解码为像素数组

#endif
