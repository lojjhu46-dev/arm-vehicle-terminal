#ifndef OVERLAY_H
#define OVERLAY_H

int parking_assist_lines_drawing(int *lcd_mp);
static void draw_line(int *lcd,
                      int x0, int y0,
                      int x1, int y1,
                      int width,
                      int color);

static void draw_rect(int *lcd,
                      int x,
                      int y,
                      int w,
                      int h,
                      int color);


#include <stdlib.h>
#include "app_config.h"
#include "overlay.h"
#define RED     0xFF0000
#define YELLOW  0xFFFF00
#define GREEN   0x00FF00
#define CAMERA_OFFSET_X 80

#endif
