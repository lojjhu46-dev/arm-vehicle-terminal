#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define TOUCH_PATH "/dev/input/event0"
#define LCD_PATH "/dev/fb0"
#define LCD_WIDTH 800
#define LCD_HEIGHT 480
#define LCD_MEM (LCD_WIDTH * LCD_HEIGHT * 4)
#define JPEG_NUM 200
// 摄像头采集图像的宽和高。
#define CAMERA_FRAME_W 640
#define CAMERA_FRAME_H 480
#define CAMERA_DISPLAY_X ((LCD_WIDTH - CAMERA_FRAME_W) / 2)

#define RECORD_ROOT_DIR "/root/workspace"
#define PHOTO_DIR RECORD_ROOT_DIR "/photos"
#define RESOURCE_DIR "./resource"

#endif
