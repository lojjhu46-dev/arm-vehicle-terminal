#ifndef LVGL_UI_H
#define LVGL_UI_H

#include "lvgl_runtime.h"
#include "member_login.h"
#include "member_sms.h"

typedef enum {
    LVGL_UI_ACTION_NONE = 0,
    LVGL_UI_ACTION_ENTER_LIVE,
    LVGL_UI_ACTION_OPEN_FILE_BROWSER
} LvglUiAction;

typedef struct {
    LvglRuntime *runtime;
    MemberLogin *member_login;
    MemberSmsWorker *sms_worker;
    int *background_pixels;
    int *back_pixels;
    int *weather_icon_pixels;
    lv_image_dsc_t background_dsc;
    lv_image_dsc_t back_dsc;
    lv_image_dsc_t weather_icon_dsc;
    lv_obj_t *screen;
    lv_obj_t *background;
    lv_obj_t *overlay;
    lv_obj_t *phone_input;
    lv_obj_t *code_input;
    lv_obj_t *keyboard;
    lv_obj_t *active_input;
    lv_obj_t *status_label;
    lv_obj_t *sms_button;
    lv_obj_t *weather_icon;
    lv_obj_t *weather_label;
    lv_font_t *font_small;
    lv_font_t *font_input;
    unsigned long dashboard_weather_version;
    unsigned long login_version;
    int dashboard_minute;
    int screen_id;
    int requested_screen;
    int syncing_login;
    LvglUiAction action;
    int initialized;
} LvglUi;

int lvgl_ui_init(LvglUi *ui, LvglRuntime *runtime, MemberLogin *member_login,
                 MemberSmsWorker *sms_worker);
void lvgl_ui_destroy(LvglUi *ui);
void lvgl_ui_process(LvglUi *ui);
void lvgl_ui_play_startup(LvglUi *ui);
void lvgl_ui_show_main(LvglUi *ui);
void lvgl_ui_show_login(LvglUi *ui);
LvglUiAction lvgl_ui_take_action(LvglUi *ui);

#endif
