#ifndef LVGL_FILE_BROWSER_H
#define LVGL_FILE_BROWSER_H

#include "lcd.h"
#include "lvgl_runtime.h"

typedef enum {
    LVGL_BROWSER_EXIT,
    LVGL_BROWSER_PLAYBACK
} LvglBrowserResult;

typedef struct {
    LvglBrowserResult result;
    char path[256];
} LvglBrowserSelection;

typedef int (*LvglBrowserInterrupt)(void *user_data);

int lvgl_file_browser_run(LvglRuntime *runtime, const char *root_dir,
                          LvglBrowserSelection *selection,
                          LvglBrowserInterrupt interrupt,
                          void *interrupt_data);

#endif
