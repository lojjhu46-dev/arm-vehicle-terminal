#include "app_config.h"
#include "overlay.h"

int parking_assist_lines_drawing(int *lcd_mp)
{
    int y_bottom = LCD_HEIGHT - 1;
    int y_top = 170;

    // 辅助线从摄像头画面左右下角向内延伸，不能覆盖两侧控制栏。
    int left_bottom = CAMERA_DISPLAY_X;
    int left_top = 280;     // 左线上端位置，越大越往中间收，延伸感越强

    for(int y = y_bottom; y >= y_top; y--)
    {
        int color = 0x00FF00; // 默认绿色

        if(y > 380 && y < 480)
            color = 0xFF0000;
        else if(y >= 280 && y <= 380)
            color = 0xFFFF00;

        // 虚线间隔
        if((y > 435 && y < 450) ||
           (y > 405 && y < 420) ||
           (y > 300 && y < 315) ||
           (y > 345 && y < 360) ||
           (y > 180 && y < 195) ||
           (y > 220 && y < 235))
        {
            continue;
        }

        // 确定线条宽度
        int width = 5;

        if((y > 420 && y < 430) || (y > 320 && y < 330))
            width = 40;
        else if(y >= 200 && y < 210)
            width = 20;
        else if(y <= 180)
            width = 170;

        // 根据 y 计算左辅助线当前 x 坐标
        int left_x = left_bottom +
                     (y_bottom - y) * (left_top - left_bottom) /
                     (y_bottom - y_top);

        // 右辅助线对称
        int right_x = CAMERA_DISPLAY_X + CAMERA_FRAME_W - 1 -
                      (left_x - CAMERA_DISPLAY_X);

        // 画左辅助线，向右加粗
        for(int i = 0; i < width; i++)
        {
            int x = left_x + i;

            if(x >= CAMERA_DISPLAY_X && x < CAMERA_DISPLAY_X + CAMERA_FRAME_W &&
               y >= 0 && y < LCD_HEIGHT)
                lcd_mp[y * LCD_WIDTH + x] = color;
        }

        // 画右辅助线，向左加粗
        for(int i = 0; i < width; i++)
        {
            int x = right_x - i;

            if(x >= CAMERA_DISPLAY_X && x < CAMERA_DISPLAY_X + CAMERA_FRAME_W &&
               y >= 0 && y < LCD_HEIGHT)
                lcd_mp[y * LCD_WIDTH + x] = color;
        }
    }

    return 0;
}
