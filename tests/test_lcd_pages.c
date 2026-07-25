#include "lcd.h"

int main(void)
{
    if (lcd_next_draw_page(0, 3) != 1)
        return 1;
    if (lcd_next_draw_page(1, 3) != 2)
        return 1;
    if (lcd_next_draw_page(2, 3) != 0)
        return 1;
    if (lcd_next_draw_page(0, 1) != 0)
        return 1;
    return 0;
}
