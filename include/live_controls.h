#ifndef LIVE_CONTROLS_H
#define LIVE_CONTROLS_H

typedef enum {
    LIVE_CONTROL_NONE = 0,
    LIVE_CONTROL_EXIT,
    LIVE_CONTROL_RECORD,
    LIVE_CONTROL_GUIDES,
    LIVE_CONTROL_PHOTO,
} LiveControl;

typedef struct {
    int *pixels;
    int width;
    int height;
} LiveControlSprite;

typedef struct {
    LiveControlSprite exit_button;
    LiveControlSprite record_start_button;
    LiveControlSprite record_stop_button;
    LiveControlSprite guides_enable_button;
    LiveControlSprite guides_disable_button;
    LiveControlSprite photo_button;
} LiveControls;

int live_controls_init(LiveControls *controls);
void live_controls_destroy(LiveControls *controls);
LiveControl live_controls_hit_test(int x, int y);
void live_controls_draw(const LiveControls *controls, int *framebuffer,
                        int recording, int guides_enabled);

#endif
