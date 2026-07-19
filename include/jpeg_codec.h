#ifndef JPEG_CODEC_H
#define JPEG_CODEC_H

#include <stdio.h>

#include "jpeglib.h"

void write_JPEG_file(const char *filename, int quality, const JSAMPLE *image_buffer);
int read_JPEG_file(const char *filename, int *lcd_mp);

#endif
