#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define TOUCH_PATH "/dev/input/event0"
#define LCD_PATH "/dev/fb0"
#define LCD_WIDTH 800  // LCD 水平方向像素数
#define LCD_HEIGHT 480 // LCD 垂直方向像素数
#define LCD_MEM (LCD_WIDTH * LCD_HEIGHT * 4) // framebuffer 总字节数
#define JPEG_NUM 200  // 录像帧最大数量
// 摄像头采集图像的宽和高。
#define CAMERA_FRAME_W 640
#define CAMERA_FRAME_H 480
#define CAMERA_DISPLAY_X ((LCD_WIDTH - CAMERA_FRAME_W) / 2)

#define RECORD_ROOT_DIR "/root/workspace"
#define RECORDINGS_DIR RECORD_ROOT_DIR "/recordings"
#define PHOTO_DIR RECORD_ROOT_DIR "/photos"
#define RESOURCE_DIR "./resource"
#define MAIN_FONT_PATH RESOURCE_DIR "/fonts/SIMHEI.TTF"
#define MAIN_WEATHER_ICON_DIR RESOURCE_DIR "/Weather"

/* TCP weather/command receiver settings. */
#ifndef WEATHER_RECEIVER_PORT
#define WEATHER_RECEIVER_PORT 8080
#endif
#ifndef WEATHER_RECEIVER_MAX_PAYLOAD
#define WEATHER_RECEIVER_MAX_PAYLOAD 8192
#endif

#endif
