#ifndef IMAGE_CONVERT_H
#define IMAGE_CONVERT_H

#include <stdio.h>

#include "jpeglib.h"

unsigned int yuyv_to_rgb_pix(int Y, int Cb, int Cr, JSAMPLE **pimage_buffer);
JSAMPLE *image_convert_frame(const char *pbuffer, unsigned int *lcd_mp);

#endif
