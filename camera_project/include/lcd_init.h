#ifndef LCD_INIT_H
#define LCD_INIT_H

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

//LCD设备驱动路径
#define  LCD_PATH   "/dev/fb0"

//LCD内存大小
#define  LCD_MEM    800*480*4

//最新改动
#define LCD_WIDTH 800
#define LCD_HEIGHT 480
//最新改动

#endif  
