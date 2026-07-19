#include <stdlib.h>
#include <unistd.h>

#include "app_config.h"
#include "image_convert.h"
#include "jpeg_codec.h"
#include "ui_flow.h"

void show_startup_animation(Lcd *lcd)
{
    char *frames[] = {
        "./logo/frame-0.jpeg", "./logo/frame-1.jpeg",
        "./logo/frame-2.jpeg", "./logo/frame-3.jpeg",
        "./logo/frame-4.jpeg", "./logo/main_UI.jpeg"
    };

    for (int round = 0; round < 5; round++)
    {
        for (int i = 0; i < 4; i++)
        {
            read_JPEG_file(frames[i], lcd->pixels);
            usleep(100 * 1000);
        }
    }
    read_JPEG_file(frames[4], lcd->pixels);
    usleep(400 * 1000);
    read_JPEG_file(frames[5], lcd->pixels);
}

//处理图像
static int mainloop(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder)
{
    //跳转到指定目录下
    chdir("/root/workspace/");
    for (;;)
    {
        touch_read(touch);
        if (camera_wait_frame(camera) < 0)
            return -1;

        //循环 出队->处理->入队
        for (unsigned int i = 0; i < camera->n_buffers; ++i)
        {
            void *frame_buffer = camera_dequeue(camera);
            if (frame_buffer == NULL)
                return -1;

            //处理图像帧并显示到LCD
            JSAMPLE *image_buffer = image_convert_frame(frame_buffer, (unsigned int *)lcd->pixels);
            if (image_buffer == NULL)
                return -1;

            // 小改动：先完成颜色转码，再编码 JPEG，确保保存的是当前摄像头帧。
            recorder_save_frame(recorder, touch->touchsrceen_x, image_buffer);
            free(image_buffer);

            //缓冲区入队
            if (camera_requeue(camera) == -1)
                return -1;
        }
        if (touch->touchsrceen_x < 80)
        {
            //点击指定区域触发回放功能
            for (int i = 0; i < JPEG_NUM; i++)
            {
                read_JPEG_file(recorder_last_path(recorder), lcd->pixels);
                usleep(30 * 1000);
            }
        }
    }
}

int ui_run(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder)
{
    int x,y;
    for(;;)
    {
        touch_read(touch);
        x = touch->touchsrceen_x;
        y = touch->touchsrceen_y;
        (void)x;
        (void)y;
        if ((touch->touchsrceen_x > 10 && touch->touchsrceen_x < 70) &&
            (touch->touchsrceen_y > 70 && touch->touchsrceen_y < 110))
        {
            //3.打开摄像头
            if (camera_open(camera, "/dev/video7") == -1)
                return -1;

            //4.初始化摄像头
            if (camera_init(camera) == -1)
                return -1;

            //5.开始采集
            if (camera_start(camera) == -1)
                return -1;

            //6.处理图像
            return mainloop(lcd, touch, camera, recorder);
        }
    }
}
