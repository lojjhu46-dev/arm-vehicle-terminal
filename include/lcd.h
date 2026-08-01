#ifndef LCD_H
#define LCD_H

#include <stddef.h>

//最新修改
#define LCD_MAX_PAGES 3

typedef struct {
    int fd;              // framebuffer 设备文件描述符
    int *pixels;         // 当前后台页像素指针
    int *base_pixels;    // mmap 映射的起始地址
    size_t map_length;   // mmap 映射长度
    int page_count;      // framebuffer 可用页面数
    int displayed_page;  // 当前显示页
    int draw_page;       // 当前后台绘制页
} Lcd;

int lcd_open(Lcd *lcd);                     // 打开 framebuffer 并初始化多页映射
void lcd_close(Lcd *lcd);                   // 关闭 framebuffer 映射并释放资源
int lcd_begin_frame(Lcd *lcd);              // 切换到下一个后台绘制页
int lcd_present(Lcd *lcd);                  // 将后台页翻页到前台显示
int lcd_show_page(Lcd *lcd, int page);      // 指定页面显示到屏幕
int lcd_page_count(const Lcd *lcd);         // 返回 framebuffer 页面数
int lcd_draw_page(const Lcd *lcd);          // 返回当前绘制页编号
int *lcd_page_pixels(const Lcd *lcd, int page); // 返回指定页面的像素地址
int lcd_next_draw_page(int displayed_page, int page_count); // 计算下一后台页
//最新修改

#endif
