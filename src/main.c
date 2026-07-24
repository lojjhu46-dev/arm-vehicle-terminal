#include <string.h>
#include <errno.h>
#include "camera.h"
#include "lcd.h"
#include "recorder.h"
#include "touch.h"
#include "ui_flow.h"

int main(void)
{
    //定义并初始化结构体用于存储lcd屏幕和触摸屏的信息
    
    Lcd lcd = { .fd = -1, .pixels = NULL };
    Touch touch = { .fd = -1, .touchsrceen_x = 0, .touchsrceen_y = 0 };
    //定义结构体存储摄像头设备节点信息
    Camera camera;
    //定义结构体存储拼接目录路径的所需信息
    Recorder recorder;
    int result;

    memset(&camera, 0, sizeof(camera));
    camera.fd = -1;
    recorder_init(&recorder);

    if (lcd_open(&lcd) == -1)
        return -1;
    if (touch_open(&touch) == -1)
    {
        lcd_close(&lcd);
        return -1;
    }
    //播放开机动画
    show_startup_animation(&lcd);
    //界面显示，切换
    result = ui_run(&lcd, &touch, &camera, &recorder);
    if(result == -1) 
    {
        fprintf(stderr,"mian ui show error:%s\n",strerror(errno));
    }
    camera_close(&camera);
    touch_close(&touch);
    lcd_close(&lcd);
    return result;
}
