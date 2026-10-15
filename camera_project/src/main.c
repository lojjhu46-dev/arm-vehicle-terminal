//最新改动
#include "camera_init.h"
#include "clinet_and_server.h"
#include "frame.h"
#include "lcd_init.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    int lcd_fd = open(LCD_PATH, O_RDWR);
    unsigned int *lcd_mp;

    if (lcd_fd < 0) {
        perror("open lcd");
        return -1;
    }
    lcd_mp = mmap(NULL, LCD_MEM, PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
    if (lcd_mp == MAP_FAILED) {
        perror("mmap lcd");
        close(lcd_fd);
        return -1;
    }
    memset(lcd_mp, 0, LCD_MEM);

    if (client_and_server_init(lcd_mp) < 0) {
        munmap(lcd_mp, LCD_MEM);
        close(lcd_fd);
        return -1;
    }
    //3.打开摄像头
    if (open_device() < 0) {
        munmap(lcd_mp, LCD_MEM);
        close(lcd_fd);
        return -1;
    }
                              
    //4.初始化摄像头
    init_device();

    //5.开始采集
    start_capturing();

    //6.处理图像
    mainloop(lcd_mp);

    //7.关闭LCD设备文件并解除映射
    close(lcd_fd);
    munmap(lcd_mp,LCD_MEM);

    return 0;
}
//最新改动
