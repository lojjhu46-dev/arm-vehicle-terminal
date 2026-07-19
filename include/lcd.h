#ifndef LCD_H
#define LCD_H

typedef struct {
    int fd;
    int *pixels;
} Lcd;

int lcd_open(Lcd *lcd);
void lcd_close(Lcd *lcd);

#endif
