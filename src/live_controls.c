#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "live_controls.h"

#define LEFT_BUTTON_X 10
#define RIGHT_BUTTON_X (LCD_WIDTH - LEFT_BUTTON_X - 60)
#define TOP_BUTTON_Y 96
#define BOTTOM_BUTTON_Y 320
#define BUTTON_WIDTH 60
#define EXIT_BUTTON_HEIGHT 64
#define GUIDES_BUTTON_HEIGHT 64
#define RECORD_BUTTON_HEIGHT 66
#define PHOTO_BUTTON_HEIGHT 60

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

int live_controls_init(LiveControls *controls)
{
    static const char *const filenames[] = {
        RESOURCE_DIR "/exit_live_view.jpg",
        RESOURCE_DIR "/start_recording.jpg",
        RESOURCE_DIR "/save_exit_recording.jpg",
        RESOURCE_DIR "/enable_parking_guides.jpg",
        RESOURCE_DIR "/disable_parking_guides.jpg",
        RESOURCE_DIR "/photo.jpg",
    };
    LiveControlSprite *const sprites[] = {
        &controls->exit_button,
        &controls->record_start_button,
        &controls->record_stop_button,
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

void live_controls_destroy(LiveControls *controls)
{
    LiveControlSprite *const sprites[] = {
        &controls->exit_button,
        &controls->record_start_button,
        &controls->record_stop_button,
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

LiveControl live_controls_hit_test(int x, int y)
{
    if (x >= LEFT_BUTTON_X && x < LEFT_BUTTON_X + BUTTON_WIDTH)
    {
        if (y >= TOP_BUTTON_Y && y < TOP_BUTTON_Y + EXIT_BUTTON_HEIGHT)
            return LIVE_CONTROL_EXIT;
        if (y >= BOTTOM_BUTTON_Y && y < BOTTOM_BUTTON_Y + GUIDES_BUTTON_HEIGHT)
            return LIVE_CONTROL_GUIDES;
    }
    if (x >= RIGHT_BUTTON_X && x < RIGHT_BUTTON_X + BUTTON_WIDTH)
    {
        if (y >= TOP_BUTTON_Y && y < TOP_BUTTON_Y + RECORD_BUTTON_HEIGHT)
            return LIVE_CONTROL_RECORD;
        if (y >= BOTTOM_BUTTON_Y && y < BOTTOM_BUTTON_Y + PHOTO_BUTTON_HEIGHT)
            return LIVE_CONTROL_PHOTO;
    }
    return LIVE_CONTROL_NONE;
}

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

void live_controls_draw(const LiveControls *controls, int *framebuffer,
                        int recording, int guides_enabled)
{
    draw_sprite(framebuffer, &controls->exit_button, LEFT_BUTTON_X, TOP_BUTTON_Y);
    draw_sprite(framebuffer, guides_enabled ? &controls->guides_disable_button :
                &controls->guides_enable_button, LEFT_BUTTON_X, BOTTOM_BUTTON_Y);
    draw_sprite(framebuffer, recording ? &controls->record_stop_button :
                &controls->record_start_button, RIGHT_BUTTON_X, TOP_BUTTON_Y);
    draw_sprite(framebuffer, &controls->photo_button, RIGHT_BUTTON_X, BOTTOM_BUTTON_Y);
}
