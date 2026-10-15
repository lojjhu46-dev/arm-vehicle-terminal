#ifndef FRAME_H
#define FRAME_H

#include <stdio.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "jpeglib.h"
#include "camera_init.h"
#include "clinet_and_server.h"
#include "lcd_init.h"

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;

//最新改动
int read_JPEG_data(const unsigned char *jpeg_buf, unsigned int jpeg_size,
                   unsigned int *lcd_mp);
int yuyv_to_rgb_pix(int Y, int Cb, int Cr);
void yuv2rgb(char *pbuffer, unsigned int *lcd_mp);
void start_capturing(void);
void mainloop(unsigned int *lcd_mp);
//最新改动

#endif
