#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "app_config.h"
#include "camera.h"
#include "freetype_lcd.h"
#include "lcd.h"
#include "main_weather.h"
#include "recorder.h"
#include "touch.h"
#include "ui_flow.h"
#include "ui_remote_commands.h"
#include "weather_receiver.h"

// 程序入口：初始化 LCD、触摸、录像与摄像头模块，显示开机动画，运行 UI 主循环
int main(void)
{
    // 定义并初始化结构体用于存储 lcd 屏幕和触摸屏信息
    Lcd lcd = { .fd = -1, .pixels = NULL };
    Touch touch = { .fd = -1, .touchsrceen_x = 0, .touchsrceen_y = 0 };
    // 定义结构体存储摄像头设备节点信息
    Camera camera;
    // 定义结构体存储录像会话与帧路径信息
    Recorder recorder;
    UiRemoteCommands remote_commands;
    int result;
    int weather_receiver_started = 0;

    memset(&camera, 0, sizeof(camera));
    camera.fd = -1;
    //最新修改
    if (recorder_init(&recorder) == -1)
        return -1;
    //最新修改

    if (lcd_open(&lcd) == -1)
    {
        recorder_destroy(&recorder);
        return -1;
    }
    if (touch_open(&touch) == -1)
    {
        lcd_close(&lcd);
        recorder_destroy(&recorder);
        return -1;
    }
    if (freetype_init(MAIN_FONT_PATH) == -1)
    {
        touch_close(&touch);
        lcd_close(&lcd);
        recorder_destroy(&recorder);
        return -1;
    }
    if (main_weather_init() == -1)
    {
        freetype_exit();
        touch_close(&touch);
        lcd_close(&lcd);
        recorder_destroy(&recorder);
        return -1;
    }
    if (ui_remote_commands_init(&remote_commands) == -1)
    {
        main_weather_destroy();
        freetype_exit();
        touch_close(&touch);
        lcd_close(&lcd);
        recorder_destroy(&recorder);
        return -1;
    }
    weather_receiver_set_text_handler(ui_remote_commands_handle_text,
                                      &remote_commands);
    if (weather_receiver_start() == 0)
        weather_receiver_started = 1;
    else
        fprintf(stderr, "weather receiver disabled: %s\n", strerror(errno));
    //界面显示，切换
    result = ui_run(&lcd, &touch, &camera, &recorder, &remote_commands);
    if(result == -1) 
    {
        fprintf(stderr,"mian ui show error:%s\n",strerror(errno));
    }
    camera_close(&camera);
    if (weather_receiver_started)
        weather_receiver_stop();
    weather_receiver_set_text_handler(NULL, NULL);
    ui_remote_commands_destroy(&remote_commands);
    //最新修改
    recorder_destroy(&recorder);
    //最新修改
    main_weather_destroy();
    freetype_exit();
    touch_close(&touch);
    lcd_close(&lcd);
    return result;
}
