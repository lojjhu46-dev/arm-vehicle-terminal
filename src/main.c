#include <string.h>

#include "camera.h"
#include "lcd.h"
#include "recorder.h"
#include "touch.h"
#include "ui_flow.h"

int main(void)
{
    Lcd lcd = { .fd = -1, .pixels = NULL };
    Touch touch = { .fd = -1, .touchsrceen_x = 0, .touchsrceen_y = 0 };
    Camera camera;
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

    show_startup_animation(&lcd);
    result = ui_run(&lcd, &touch, &camera, &recorder);

    camera_close(&camera);
    touch_close(&touch);
    lcd_close(&lcd);
    return result;
}
