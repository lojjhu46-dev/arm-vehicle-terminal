#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "live_controls.h"

#define LEFT_BUTTON_X 10
#define RIGHT_BUTTON_X (LCD_WIDTH - LEFT_BUTTON_X - 60)
#define TOP_BUTTON_Y 96
#define MIDDLE_BUTTON_Y 208
#define BOTTOM_BUTTON_Y 320
#define BUTTON_WIDTH 60
#define EXIT_BUTTON_HEIGHT 64
#define FILE_MANAGER_BUTTON_HEIGHT 64
#define GUIDES_BUTTON_HEIGHT 64
#define RECORD_BUTTON_HEIGHT 66
#define VIEW_RECORDINGS_BUTTON_HEIGHT 64
#define PHOTO_BUTTON_HEIGHT 60

// 从 JPEG 文件加载按钮图像到 sprite 结构体中
// 仅接受宽度为 BUTTON_WIDTH 且高度不超过 68 的图像
static int load_sprite(LiveControlSprite *sprite, const char *filename)
{
    if (read_JPEG_image(filename, &sprite->pixels, &sprite->width, &sprite->height) == -1)
        return -1;
    if (sprite->width != BUTTON_WIDTH || sprite->height > 68)
    {
        free(sprite->pixels);
        memset(sprite, 0, sizeof(*sprite));
        return -1;
    }
    return 0;
}

// 初始化实时控制按钮，加载所有按钮图像资源
// 如果任意图像加载失败，则释放已加载资源并返回错误
int live_controls_init(LiveControls *controls)
{
    static const char *const filenames[] = {
        RESOURCE_DIR "/exit_live_view.jpg",
        RESOURCE_DIR "/file_manager.jpg",
        RESOURCE_DIR "/start_recording.jpg",
        RESOURCE_DIR "/save_exit_recording.jpg",
        RESOURCE_DIR "/view_recordings.jpg",
        RESOURCE_DIR "/enable_parking_guides.jpg",
        RESOURCE_DIR "/disable_parking_guides.jpg",
        RESOURCE_DIR "/photo.jpg",
    };
    LiveControlSprite *const sprites[] = {
        &controls->exit_button,
        &controls->file_manager_button,
        &controls->record_start_button,
        &controls->record_stop_button,
        &controls->view_recordings_button,
        &controls->guides_enable_button,
        &controls->guides_disable_button,
        &controls->photo_button,
    };

    memset(controls, 0, sizeof(*controls));
    for (size_t i = 0; i < sizeof(sprites) / sizeof(sprites[0]); ++i)
    {
        if (load_sprite(sprites[i], filenames[i]) == -1)
        {
            live_controls_destroy(controls);
            return -1;
        }
    }
    return 0;
}

// 释放实时控制按钮占用的图像内存，并重置按钮结构体
void live_controls_destroy(LiveControls *controls)
{
    LiveControlSprite *const sprites[] = {
        &controls->exit_button,
        &controls->file_manager_button,
        &controls->record_start_button,
        &controls->record_stop_button,
        &controls->view_recordings_button,
        &controls->guides_enable_button,
        &controls->guides_disable_button,
        &controls->photo_button,
    };

    for (size_t i = 0; i < sizeof(sprites) / sizeof(sprites[0]); ++i)
    {
        free(sprites[i]->pixels);
        memset(sprites[i], 0, sizeof(*sprites[i]));
    }
}

// 判断触摸坐标是否落在某个实时控制按钮上
// 根据 x/y 坐标范围返回对应的按钮枚举
LiveControl live_controls_hit_test(int x, int y)
{
    if (x >= LEFT_BUTTON_X && x < LEFT_BUTTON_X + BUTTON_WIDTH)
    {
        if (y >= TOP_BUTTON_Y && y < TOP_BUTTON_Y + EXIT_BUTTON_HEIGHT)
            return LIVE_CONTROL_EXIT;
        if (y >= MIDDLE_BUTTON_Y && y < MIDDLE_BUTTON_Y + FILE_MANAGER_BUTTON_HEIGHT)
            return LIVE_CONTROL_FILE_MANAGER;
        if (y >= BOTTOM_BUTTON_Y && y < BOTTOM_BUTTON_Y + GUIDES_BUTTON_HEIGHT)
            return LIVE_CONTROL_GUIDES;
    }
    if (x >= RIGHT_BUTTON_X && x < RIGHT_BUTTON_X + BUTTON_WIDTH)
    {
        if (y >= TOP_BUTTON_Y && y < TOP_BUTTON_Y + RECORD_BUTTON_HEIGHT)
            return LIVE_CONTROL_RECORD;
        if (y >= MIDDLE_BUTTON_Y && y < MIDDLE_BUTTON_Y + VIEW_RECORDINGS_BUTTON_HEIGHT)
            return LIVE_CONTROL_VIEW_RECORDINGS;
        if (y >= BOTTOM_BUTTON_Y && y < BOTTOM_BUTTON_Y + PHOTO_BUTTON_HEIGHT)
            return LIVE_CONTROL_PHOTO;
    }
    return LIVE_CONTROL_NONE;
}

// 将按钮图像绘制到屏幕帧缓冲区，支持边界裁剪
static void draw_sprite(int *framebuffer, const LiveControlSprite *sprite, int x, int y)
{
    for (int row = 0; row < sprite->height; ++row)
    {
        for (int column = 0; column < sprite->width; ++column)
        {
            int target_x = x + column;
            int target_y = y + row;

            if (target_x >= 0 && target_x < LCD_WIDTH &&
                target_y >= 0 && target_y < LCD_HEIGHT)
                framebuffer[target_y * LCD_WIDTH + target_x] =
                    sprite->pixels[row * sprite->width + column];
        }
    }
}

//最新修改
// 清除首次进入实时画面时遗留的主界面侧栏像素。
// 只清除摄像头显示区两侧的边栏区域，不修改中央视频区
static void clear_sidebars(int *framebuffer)
{
    for (int y = 0; y < LCD_HEIGHT; ++y)
    {
        for (int x = 0; x < CAMERA_DISPLAY_X; ++x)
            framebuffer[y * LCD_WIDTH + x] = 0x000000;
        for (int x = CAMERA_DISPLAY_X + CAMERA_FRAME_W; x < LCD_WIDTH; ++x)
            framebuffer[y * LCD_WIDTH + x] = 0x000000;
    }
}

// 清除按钮区域，保留右上角 REC 指示器。
// 这里清除左侧和右侧按钮的整个显示矩形区域，避免残留旧按钮图像
static void clear_button_areas(int *framebuffer)
{
    for (int y = TOP_BUTTON_Y; y < BOTTOM_BUTTON_Y + 68; ++y)
    {
        for (int x = LEFT_BUTTON_X; x < LEFT_BUTTON_X + BUTTON_WIDTH; ++x)
            framebuffer[y * LCD_WIDTH + x] = 0x000000;
        for (int x = RIGHT_BUTTON_X; x < RIGHT_BUTTON_X + BUTTON_WIDTH; ++x)
            framebuffer[y * LCD_WIDTH + x] = 0x000000;
    }
}

// 仅在首次显示或按钮状态变化时刷新侧栏。
// 绘制实时控制界面按钮，根据状态选择合适图像
// initial_draw 为真时清除侧边栏，否则只刷新按钮区域
void live_controls_draw(const LiveControls *controls, int *framebuffer,
                        int recording, int guides_enabled, int initial_draw)
{
    if (initial_draw)
        clear_sidebars(framebuffer);
    else
        clear_button_areas(framebuffer);
    draw_sprite(framebuffer, &controls->exit_button, LEFT_BUTTON_X, TOP_BUTTON_Y);
    draw_sprite(framebuffer, &controls->file_manager_button, LEFT_BUTTON_X,
                MIDDLE_BUTTON_Y);
    draw_sprite(framebuffer, guides_enabled ? &controls->guides_disable_button :
                &controls->guides_enable_button, LEFT_BUTTON_X, BOTTOM_BUTTON_Y);
    draw_sprite(framebuffer, recording ? &controls->record_stop_button :
                &controls->record_start_button, RIGHT_BUTTON_X, TOP_BUTTON_Y);
    draw_sprite(framebuffer, &controls->view_recordings_button, RIGHT_BUTTON_X,
                MIDDLE_BUTTON_Y);
    draw_sprite(framebuffer, &controls->photo_button, RIGHT_BUTTON_X, BOTTOM_BUTTON_Y);
}

// 仅绘制回放界面所需的退出按钮。
void live_controls_draw_playback(const LiveControls *controls, int *framebuffer)
{
    clear_sidebars(framebuffer);
    draw_sprite(framebuffer, &controls->exit_button, LEFT_BUTTON_X, TOP_BUTTON_Y);
}
//最新修改
