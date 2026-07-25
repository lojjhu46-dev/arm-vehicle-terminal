#ifndef LCD_H
#define LCD_H

#include <stddef.h>

//最新修改
#define LCD_MAX_PAGES 3

typedef struct {
    int fd;
    int *pixels;
    int *base_pixels;
    size_t map_length;
    int page_count;
    int displayed_page;
    int draw_page;
} Lcd;

int lcd_open(Lcd *lcd);
void lcd_close(Lcd *lcd);
int lcd_begin_frame(Lcd *lcd);
int lcd_present(Lcd *lcd);
int lcd_show_page(Lcd *lcd, int page);
int lcd_page_count(const Lcd *lcd);
int lcd_draw_page(const Lcd *lcd);
int *lcd_page_pixels(const Lcd *lcd, int page);
int lcd_next_draw_page(int displayed_page, int page_count);
//最新修改

#endif
