#include "lcd_init.h"

void *lcd_init()
{
//1.打开LCD设备驱动
    int lcd_fd = open(LCD_PATH,O_RDWR);
    //错误处理
    if(-1 == lcd_fd)
    {
        perror("open lcd device error\r\n");
        return -1;
    }

    //2.对LCD设备文件进行内存映射，目的是提高LCD刷新效率地址，
    unsigned int *lcd_mp = mmap(   
                                    NULL,                       //待申请的内存由kernel自动分配
                                    LCD_MEM,                    //待申请的内存大小
                                    PROT_READ|PROT_WRITE,       //权限是可读可写
                                    MAP_SHARED,                 //共享内存，共享给其他进程
                                    lcd_fd,                     //待映射的文件句柄，前提是要open
                                    0                           //地址偏移，不偏移
                              );
}