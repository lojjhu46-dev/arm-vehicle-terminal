#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "app_config.h"
#include "lcd.h"

int lcd_open(Lcd *lcd)
{
    lcd->fd = open(LCD_PATH, O_RDWR);
    if (lcd->fd == -1)
    {
        perror("open fb0 fail");
        return -1;
    }

    lcd->pixels = mmap(NULL, LCD_MEM, PROT_WRITE | PROT_READ, MAP_SHARED, lcd->fd, 0);
    if (lcd->pixels == MAP_FAILED)
    {
        perror("mmap fb0 fail");
        close(lcd->fd);
        lcd->fd = -1;
        return -1;
    }

    return 0;
}

void lcd_close(Lcd *lcd)
{
    if (lcd->pixels != NULL && lcd->pixels != MAP_FAILED)
        munmap(lcd->pixels, LCD_MEM);
    if (lcd->fd != -1)
        close(lcd->fd);
}
