#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "app_config.h"
#include "lcd.h"

//最新修改
#define LCD_PAGE_PIXELS (LCD_WIDTH * LCD_HEIGHT)
#define LCD_PAGE_BYTES (LCD_PAGE_PIXELS * (int)sizeof(int))

// 选择当前显示页之后的空闲页面作为后台页。
int lcd_next_draw_page(int displayed_page, int page_count)
{
    if (page_count <= 1)
        return 0;
    return (displayed_page + 1) % page_count;
}

// 获取指定页面的 framebuffer 像素地址。
int *lcd_page_pixels(const Lcd *lcd, int page)
{
    if (page < 0 || page >= lcd->page_count)
        return NULL;
    return lcd->base_pixels + page * LCD_PAGE_PIXELS;
}

// 返回可用 framebuffer 页面数。
int lcd_page_count(const Lcd *lcd)
{
    return lcd->page_count;
}

// 返回当前后台绘制页编号。
int lcd_draw_page(const Lcd *lcd)
{
    return lcd->draw_page;
}

// 将硬件切换到指定虚拟页面。
int lcd_show_page(Lcd *lcd, int page)
{
    struct fb_var_screeninfo variable_info;

    if (page < 0 || page >= lcd->page_count)
        return -1;
    if (lcd->page_count == 1)
    {
        lcd->displayed_page = 0;
        lcd->draw_page = 0;
        lcd->pixels = lcd->base_pixels;
        return 0;
    }
    if (ioctl(lcd->fd, FBIOGET_VSCREENINFO, &variable_info) == -1)
        return -1;
    variable_info.yoffset = page * LCD_HEIGHT;
    variable_info.activate = FB_ACTIVATE_VBL;
    if (ioctl(lcd->fd, FBIOPAN_DISPLAY, &variable_info) == -1)
    {
        perror("FBIOPAN_DISPLAY failed, fall back to single framebuffer");
        lcd->page_count = 1;
        lcd->displayed_page = 0;
        lcd->draw_page = 0;
        lcd->pixels = lcd->base_pixels;
        return 0;
    }
    lcd->displayed_page = page;
    lcd->draw_page = lcd_next_draw_page(page, lcd->page_count);
    lcd->pixels = lcd_page_pixels(lcd, lcd->draw_page);
    return 0;
}

// 选择后台页，供完整实时帧绘制。
int lcd_begin_frame(Lcd *lcd)
{
    lcd->draw_page = lcd_next_draw_page(lcd->displayed_page, lcd->page_count);
    lcd->pixels = lcd_page_pixels(lcd, lcd->draw_page);
    return lcd->pixels == NULL ? -1 : 0;
}

// 提交已绘制的后台页。
int lcd_present(Lcd *lcd)
{
    return lcd_show_page(lcd, lcd->draw_page);
}

// 打开 framebuffer 并优先启用三页映射。
int lcd_open(Lcd *lcd)
{
    struct fb_fix_screeninfo fixed_info;
    struct fb_var_screeninfo variable_info;
    int available_pages;

    memset(lcd, 0, sizeof(*lcd));
    lcd->fd = open(LCD_PATH, O_RDWR);
    if (lcd->fd == -1)
    {
        perror("open fb0 fail");
        return -1;
    }
    if (ioctl(lcd->fd, FBIOGET_VSCREENINFO, &variable_info) == -1 ||
        ioctl(lcd->fd, FBIOGET_FSCREENINFO, &fixed_info) == -1)
    {
        perror("get framebuffer info fail");
        close(lcd->fd);
        lcd->fd = -1;
        return -1;
    }
    if (variable_info.xres != LCD_WIDTH || variable_info.yres != LCD_HEIGHT ||
        variable_info.bits_per_pixel != 32 || fixed_info.line_length != LCD_WIDTH * 4)
    {
        fprintf(stderr, "unsupported framebuffer: %ux%u, %u bpp, stride %u\n",
                variable_info.xres, variable_info.yres, variable_info.bits_per_pixel,
                fixed_info.line_length);
        close(lcd->fd);
        lcd->fd = -1;
        return -1;
    }

    available_pages = variable_info.yres_virtual / LCD_HEIGHT;
    lcd->page_count = available_pages >= LCD_MAX_PAGES ? LCD_MAX_PAGES : 1;
    lcd->map_length = fixed_info.smem_len;
    if (lcd->map_length < (size_t)lcd->page_count * LCD_PAGE_BYTES)
    {
        close(lcd->fd);
        lcd->fd = -1;
        return -1;
    }
    lcd->base_pixels = mmap(NULL, lcd->map_length, PROT_WRITE | PROT_READ,
                            MAP_SHARED, lcd->fd, 0);
    if (lcd->base_pixels == MAP_FAILED)
    {
        perror("mmap fb0 fail");
        close(lcd->fd);
        lcd->base_pixels = NULL;
        lcd->fd = -1;
        return -1;
    }
    lcd->displayed_page = variable_info.yoffset / LCD_HEIGHT;
    if (lcd->displayed_page >= lcd->page_count)
        lcd->displayed_page = 0;
    lcd->draw_page = lcd_next_draw_page(lcd->displayed_page, lcd->page_count);
    lcd->pixels = lcd_page_pixels(lcd, lcd->draw_page);
    return 0;
}

// 解除完整 framebuffer 映射。
void lcd_close(Lcd *lcd)
{
    if (lcd->base_pixels != NULL && lcd->base_pixels != MAP_FAILED)
        munmap(lcd->base_pixels, lcd->map_length);
    if (lcd->fd != -1)
        close(lcd->fd);
    lcd->base_pixels = NULL;
    lcd->pixels = NULL;
    lcd->map_length = 0;
    lcd->fd = -1;
}
//最新修改
