#ifndef LVGL_FILE_BROWSER_H
#define LVGL_FILE_BROWSER_H

#include "lcd.h"
#include "touch.h"

typedef enum {
    LVGL_BROWSER_EXIT,
    LVGL_BROWSER_PLAYBACK
} LvglBrowserResult;

typedef struct {
    LvglBrowserResult result;
    char path[256];
} LvglBrowserSelection;

int lvgl_file_browser_run(Lcd *lcd, Touch *touch, const char *root_dir,
                          LvglBrowserSelection *selection);

#endif
