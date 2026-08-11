#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "freetype_lcd.h"
#include "jpeg_codec.h"
#include "lvgl_ui.h"
#include "main_weather.h"

enum {
    LVGL_UI_SCREEN_MAIN = 0,
    LVGL_UI_SCREEN_EXPAND,
    LVGL_UI_SCREEN_LOGIN,
    LVGL_UI_SCREEN_SANCTUARY
};

#define BACK_BUTTON_X 5
#define BACK_BUTTON_Y 5
#define BACK_BUTTON_WIDTH 60
#define BACK_BUTTON_HEIGHT 68
#define KEYPAD_DELETE_LABEL "×"
#define KEYPAD_SUBMIT_LABEL "回车"

static const char *const numeric_keyboard_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    KEYPAD_DELETE_LABEL, "0", KEYPAD_SUBMIT_LABEL, ""
};

static const lv_buttonmatrix_ctrl_t numeric_keyboard_ctrl[] = {
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG,
    LV_BUTTONMATRIX_CTRL_WIDTH_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG
};

static time_t current_minute(void)
{
    struct timeval now;

    if (gettimeofday(&now, NULL) == -1)
        return -1;
    return now.tv_sec / 60;
}

static void image_descriptor_init(lv_image_dsc_t *descriptor, int *pixels,
                                  int width, int height)
{
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->header.magic = LV_IMAGE_HEADER_MAGIC;
    descriptor->header.cf = LV_COLOR_FORMAT_XRGB8888;
    descriptor->header.w = (uint16_t)width;
    descriptor->header.h = (uint16_t)height;
    descriptor->header.stride = (uint16_t)(width * (int)sizeof(*pixels));
    descriptor->data_size = (uint32_t)((size_t)width * (size_t)height *
                                       sizeof(*pixels));
    descriptor->data = (const uint8_t *)pixels;
}

static int load_image_pixels(const char *path, int **pixels, int expected_width,
                             int expected_height)
{
    int *loaded = NULL;
    int width = 0;
    int height = 0;

    if (read_JPEG_image(path, &loaded, &width, &height) == -1 ||
        width != expected_width || height != expected_height)
    {
        fprintf(stderr, "[LVGL] invalid image %s (expected %dx%d, got %dx%d)\n",
                path, expected_width, expected_height, width, height);
        free(loaded);
        return -1;
    }
    *pixels = loaded;
    return 0;
}

static int load_background(LvglUi *ui, const char *path)
{
    int *loaded = NULL;

    if (load_image_pixels(path, &loaded, LCD_WIDTH, LCD_HEIGHT) == -1)
        return -1;
    memcpy(ui->background_pixels, loaded,
           LCD_WIDTH * LCD_HEIGHT * sizeof(*loaded));
    free(loaded);
    lv_image_set_src(ui->background, &ui->background_dsc);
    return 0;
}

/* Startup frames are legacy assets with a shorter height than the LCD. */
static int load_startup_background(LvglUi *ui, const char *path)
{
    int *loaded = NULL;
    int width = 0;
    int height = 0;

    if (read_JPEG_image(path, &loaded, &width, &height) == -1)
    {
        fprintf(stderr, "[LVGL] cannot load startup frame %s\n", path);
        return -1;
    }
    if (width <= 0 || width > LCD_WIDTH || height <= 0 || height > LCD_HEIGHT)
    {
        fprintf(stderr, "[LVGL] startup frame %s has unsupported size %dx%d\n",
                path, width, height);
        free(loaded);
        return -1;
    }

    memset(ui->background_pixels, 0,
           (size_t)LCD_WIDTH * (size_t)LCD_HEIGHT *
               sizeof(*ui->background_pixels));
    for (int y = 0; y < height; ++y)
    {
        memcpy(ui->background_pixels + (size_t)y * LCD_WIDTH,
               loaded + (size_t)y * width,
               (size_t)width * sizeof(*ui->background_pixels));
    }
    free(loaded);
    lv_image_set_src(ui->background, &ui->background_dsc);
    return 0;
}

static lv_obj_t *create_transparent_button(lv_obj_t *parent, int x, int y,
                                           int width, int height,
                                           lv_event_cb_t callback,
                                           void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_white(), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_30, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    return button;
}

static void show_screen(LvglUi *ui, int screen);

static void show_main_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);

    ui->requested_screen = LVGL_UI_SCREEN_MAIN;
}

static void show_expand_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);

    ui->requested_screen = LVGL_UI_SCREEN_EXPAND;
}

static void show_login_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);

    member_login_reset(ui->member_login);
    ui->requested_screen = LVGL_UI_SCREEN_LOGIN;
}

static void enter_live_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);

    ui->action = LVGL_UI_ACTION_ENTER_LIVE;
}

static void open_file_browser_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);

    ui->action = LVGL_UI_ACTION_OPEN_FILE_BROWSER;
}

static void back_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);

    if (ui->screen_id == LVGL_UI_SCREEN_LOGIN)
        member_login_reset(ui->member_login);
    if (ui->screen_id == LVGL_UI_SCREEN_SANCTUARY)
        ui->requested_screen = LVGL_UI_SCREEN_LOGIN;
    else if (ui->screen_id == LVGL_UI_SCREEN_LOGIN)
        ui->requested_screen = LVGL_UI_SCREEN_EXPAND;
    else
        ui->requested_screen = LVGL_UI_SCREEN_MAIN;
}

static MemberLoginField input_field(const LvglUi *ui, const lv_obj_t *input)
{
    return input == ui->phone_input ? MEMBER_LOGIN_FIELD_PHONE :
                                     MEMBER_LOGIN_FIELD_CODE;
}

static void set_input_appearance(lv_obj_t *input, int active)
{
    if (input == NULL)
        return;
    if (active)
    {
        lv_obj_set_style_bg_color(input, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(input, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(input, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(input, lv_color_black(), LV_PART_CURSOR);
        lv_obj_add_state(input, LV_STATE_FOCUSED);
    }
    else
    {
        lv_obj_set_style_bg_opa(input, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_color(input, lv_color_hex(0xe8edf7), LV_PART_MAIN);
        lv_obj_set_style_bg_color(input, lv_color_hex(0xe8edf7), LV_PART_CURSOR);
        lv_obj_remove_state(input, LV_STATE_FOCUSED);
    }
}

// Keep model selection, cursor placement and active-field appearance in sync.
static void activate_input(LvglUi *ui, lv_obj_t *input)
{
    if (ui == NULL || input == NULL ||
        (input != ui->phone_input && input != ui->code_input))
        return;
    ui->active_input = input;
    set_input_appearance(ui->phone_input, input == ui->phone_input);
    set_input_appearance(ui->code_input, input == ui->code_input);
    lv_textarea_set_cursor_pos(input, LV_TEXTAREA_CURSOR_LAST);
    (void)member_login_set_input(ui->member_login, input_field(ui, input),
                                 lv_textarea_get_text(input));
}

static void login_sync(LvglUi *ui)
{
    MemberLoginSnapshot snapshot;

    if (ui->screen_id != LVGL_UI_SCREEN_LOGIN)
        return;
    member_login_snapshot(ui->member_login, &snapshot);
    if (snapshot.version == ui->login_version)
        return;
    ui->syncing_login = 1;
    if (strcmp(lv_textarea_get_text(ui->phone_input), snapshot.phone) != 0)
        lv_textarea_set_text(ui->phone_input, snapshot.phone);
    if (strcmp(lv_textarea_get_text(ui->code_input), snapshot.code) != 0)
        lv_textarea_set_text(ui->code_input, snapshot.code);
    lv_label_set_text(ui->status_label, snapshot.status);
    if (snapshot.sms_pending)
        lv_obj_add_state(ui->sms_button, LV_STATE_DISABLED);
    else
        lv_obj_remove_state(ui->sms_button, LV_STATE_DISABLED);
    activate_input(ui, snapshot.active_field == MEMBER_LOGIN_FIELD_CODE ?
                       ui->code_input : ui->phone_input);
    ui->syncing_login = 0;
    ui->login_version = snapshot.version;
}

static void input_value_changed_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);
    lv_obj_t *textarea = lv_event_get_target(event);
    MemberLoginField field = input_field(ui, textarea);

    if (!ui->syncing_login)
        (void)member_login_set_input(ui->member_login, field,
                                     lv_textarea_get_text(textarea));
}

static void input_activate_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);

    activate_input(ui, lv_event_get_target(event));
}

static void submit_login(LvglUi *ui)
{
    if (member_login_submit(ui->member_login) == MEMBER_LOGIN_ACTION_SUCCESS)
    {
        member_login_reset(ui->member_login);
        ui->requested_screen = LVGL_UI_SCREEN_SANCTUARY;
    }
}

static void login_event_cb(lv_event_t *event)
{
    submit_login(lv_event_get_user_data(event));
}

static void keyboard_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);
    uint32_t button = lv_buttonmatrix_get_selected_button(ui->keyboard);
    const char *text;

    if (ui->active_input == NULL || button == LV_BUTTONMATRIX_BUTTON_NONE)
        return;
    text = lv_buttonmatrix_get_button_text(ui->keyboard, button);
    if (text == NULL)
        return;
    if (strcmp(text, KEYPAD_DELETE_LABEL) == 0)
        lv_textarea_delete_char(ui->active_input);
    else if (strcmp(text, KEYPAD_SUBMIT_LABEL) == 0)
        submit_login(ui);
    else if (text[0] >= '0' && text[0] <= '9' && text[1] == '\0')
        lv_textarea_add_text(ui->active_input, text);
    lv_textarea_set_cursor_pos(ui->active_input, LV_TEXTAREA_CURSOR_LAST);
}

static void sms_event_cb(lv_event_t *event)
{
    LvglUi *ui = lv_event_get_user_data(event);
    MemberSmsRequest request;

    if (member_login_request_sms(ui->member_login, &request) ==
        MEMBER_LOGIN_ACTION_SEND_SMS)
    {
        activate_input(ui, ui->code_input);
        if (member_sms_worker_start(ui->sms_worker, ui->member_login, &request) == -1)
            member_login_set_sms_start_failed(ui->member_login, &request);
    }
}

static void draw_main_dashboard(LvglUi *ui)
{
    static const char *const weekdays[] = {
        "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"
    };
    struct timeval now;
    struct tm local_time;
    MainWeather weather;
    unsigned long weather_version;
    char clock_text[8];
    char date_text[32];
    char date_part[16];
    char temperature_text[16];

    if (load_background(ui, RESOURCE_DIR "/logo/mian_UI.jpeg") == -1 ||
        gettimeofday(&now, NULL) == -1 ||
        localtime_r(&now.tv_sec, &local_time) == NULL ||
        main_weather_get(&weather, &weather_version) == -1)
        return;
    if (strftime(clock_text, sizeof(clock_text), "%H:%M", &local_time) == 0 ||
        strftime(date_part, sizeof(date_part), "%Y/%m/%d", &local_time) == 0)
        return;
    snprintf(date_text, sizeof(date_text), "%s  %s", date_part,
             weekdays[local_time.tm_wday]);
    snprintf(temperature_text, sizeof(temperature_text), "%d℃",
             weather.temperature_c);
    freetype_set_lcd((unsigned int *)ui->background_pixels);
    freetype_set_size(52);
    lcd_draw_text(120, 105, clock_text, 0xf4f4f4);
    freetype_set_size(18);
    lcd_draw_text(123, 129, date_text, 0x9c9c9c);
    freetype_set_size(54);
    lcd_draw_text(558, 105, temperature_text, 0xf4f4f4);
    if (ui->weather_icon != NULL && ui->weather_label != NULL &&
        main_weather_copy_icon_pixels(ui->weather_icon_pixels, &weather) == 0)
    {
        char weather_text[96];

        snprintf(weather_text, sizeof(weather_text), "%s | %s", weather.city,
                 main_weather_label(weather.type));
        lv_image_set_src(ui->weather_icon, &ui->weather_icon_dsc);
        lv_obj_invalidate(ui->weather_icon);
        lv_label_set_text(ui->weather_label, weather_text);
    }
    ui->dashboard_minute = (int)(now.tv_sec / 60);
    ui->dashboard_weather_version = weather_version;
    lv_obj_invalidate(ui->background);
}

static void create_back_button(LvglUi *ui)
{
    lv_obj_t *image = lv_image_create(ui->overlay);

    lv_image_set_src(image, &ui->back_dsc);
    lv_obj_set_pos(image, BACK_BUTTON_X, BACK_BUTTON_Y);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    (void)create_transparent_button(ui->overlay, BACK_BUTTON_X, BACK_BUTTON_Y,
                                    BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT,
                                    back_event_cb, ui);
}

static void create_main_weather_widgets(LvglUi *ui)
{
    ui->weather_icon = lv_image_create(ui->overlay);
    lv_image_set_src(ui->weather_icon, &ui->weather_icon_dsc);
    lv_obj_set_pos(ui->weather_icon, 352, 68);
    lv_obj_remove_flag(ui->weather_icon, LV_OBJ_FLAG_CLICKABLE);

    ui->weather_label = lv_label_create(ui->overlay);
    lv_obj_set_pos(ui->weather_label, 565, 116);
    lv_obj_set_size(ui->weather_label, 220, 26);
    lv_label_set_long_mode(ui->weather_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(ui->weather_label, lv_color_hex(0x9c9c9c), 0);
    lv_obj_set_style_text_font(ui->weather_label, ui->font_small, 0);
}

static void style_textarea(LvglUi *ui, lv_obj_t *textarea)
{
    lv_obj_set_style_bg_opa(textarea, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(textarea, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(textarea, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(textarea, 34, LV_PART_MAIN);
    lv_obj_set_style_pad_top(textarea, 9, LV_PART_MAIN);
    lv_obj_set_style_text_color(textarea, lv_color_hex(0xe8edf7), LV_PART_MAIN);
    lv_obj_set_style_text_color(textarea, lv_color_hex(0x8c9bad),
                                LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_text_opa(textarea, LV_OPA_COVER,
                              LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(textarea, lv_color_hex(0xe8edf7), LV_PART_CURSOR);
    lv_obj_set_style_text_font(textarea, ui->font_small, LV_PART_MAIN);
    lv_obj_set_style_text_font(textarea, ui->font_small,
                               LV_PART_TEXTAREA_PLACEHOLDER);
}

static void create_login_page(LvglUi *ui)
{
    if (load_background(ui, RESOURCE_DIR "/Menberslogin.jpg") == -1)
        return;
    create_back_button(ui);
    ui->phone_input = lv_textarea_create(ui->overlay);
    lv_obj_set_pos(ui->phone_input, 383, 105);
    lv_obj_set_size(ui->phone_input, 352, 41);
    lv_textarea_set_one_line(ui->phone_input, true);
    lv_textarea_set_max_length(ui->phone_input, MEMBER_LOGIN_PHONE_LENGTH);
    lv_textarea_set_accepted_chars(ui->phone_input, "0123456789");
    lv_textarea_set_placeholder_text(ui->phone_input, "请输入手机号");
    style_textarea(ui, ui->phone_input);
    lv_obj_add_event_cb(ui->phone_input, input_value_changed_cb,
                        LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_event_cb(ui->phone_input, input_activate_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(ui->phone_input, input_activate_cb, LV_EVENT_FOCUSED, ui);

    ui->code_input = lv_textarea_create(ui->overlay);
    lv_obj_set_pos(ui->code_input, 383, 160);
    lv_obj_set_size(ui->code_input, 267, 40);
    lv_textarea_set_one_line(ui->code_input, true);
    lv_textarea_set_max_length(ui->code_input, MEMBER_LOGIN_CODE_LENGTH);
    lv_textarea_set_accepted_chars(ui->code_input, "0123456789");
    lv_textarea_set_placeholder_text(ui->code_input, "请输入手机验证码");
    style_textarea(ui, ui->code_input);
    lv_obj_add_event_cb(ui->code_input, input_value_changed_cb,
                        LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_event_cb(ui->code_input, input_activate_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(ui->code_input, input_activate_cb, LV_EVENT_FOCUSED, ui);

    ui->sms_button = create_transparent_button(ui->overlay, 650, 160, 85, 40,
                                               sms_event_cb, ui);
    (void)create_transparent_button(ui->overlay, 383, 213, 352, 42,
                                    login_event_cb, ui);

    ui->keyboard = lv_buttonmatrix_create(ui->overlay);
    lv_obj_set_pos(ui->keyboard, 363, 301);
    lv_obj_set_size(ui->keyboard, 391, 157);
    lv_buttonmatrix_set_map(ui->keyboard, numeric_keyboard_map);
    lv_buttonmatrix_set_ctrl_map(ui->keyboard, numeric_keyboard_ctrl);
    lv_obj_remove_flag(ui->keyboard, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_style_bg_opa(ui->keyboard, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->keyboard, lv_color_hex(0xf1f4f8), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(ui->keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(ui->keyboard, lv_color_hex(0xdce5ef),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->keyboard, lv_color_hex(0x20242a), LV_PART_ITEMS);
    lv_obj_set_style_text_font(ui->keyboard, ui->font_input, LV_PART_ITEMS);
    lv_obj_set_style_border_width(ui->keyboard, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(ui->keyboard, 6, LV_PART_ITEMS);
    lv_obj_add_event_cb(ui->keyboard, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, ui);

    ui->status_label = lv_label_create(ui->overlay);
    lv_obj_set_pos(ui->status_label, 383, 263);
    lv_obj_set_size(ui->status_label, 352, 28);
    lv_obj_set_style_text_color(ui->status_label, lv_color_hex(0xffc36d), 0);
    lv_obj_set_style_text_font(ui->status_label, ui->font_small, 0);
    ui->login_version = 0;
    login_sync(ui);
}

static void show_screen(LvglUi *ui, int screen)
{
    lv_obj_clean(ui->overlay);
    ui->phone_input = NULL;
    ui->code_input = NULL;
    ui->keyboard = NULL;
    ui->active_input = NULL;
    ui->status_label = NULL;
    ui->sms_button = NULL;
    ui->weather_icon = NULL;
    ui->weather_label = NULL;
    ui->screen_id = screen;
    ui->requested_screen = -1;

    switch (screen)
    {
    case LVGL_UI_SCREEN_MAIN:
        create_main_weather_widgets(ui);
        draw_main_dashboard(ui);
        (void)create_transparent_button(ui->overlay, 45, 160, 115, 145,
                                        enter_live_event_cb, ui);
        (void)create_transparent_button(ui->overlay, 195, 160, 115, 145,
                                        open_file_browser_event_cb, ui);
        (void)create_transparent_button(ui->overlay, 635, 325, 135, 140,
                                        show_expand_event_cb, ui);
        break;
    case LVGL_UI_SCREEN_EXPAND:
        if (load_background(ui, RESOURCE_DIR "/expand.jpg") == 0)
        {
            create_back_button(ui);
            (void)create_transparent_button(ui->overlay, 345, 375, 240, 60,
                                            show_login_event_cb, ui);
            lv_obj_invalidate(ui->background);
        }
        break;
    case LVGL_UI_SCREEN_LOGIN:
        create_login_page(ui);
        lv_obj_invalidate(ui->background);
        break;
    case LVGL_UI_SCREEN_SANCTUARY:
        if (load_background(ui, RESOURCE_DIR "/MembersSanctuary.jpg") == 0)
        {
            create_back_button(ui);
            lv_obj_invalidate(ui->background);
        }
        break;
    }
}

int lvgl_ui_init(LvglUi *ui, LvglRuntime *runtime, MemberLogin *member_login,
                 MemberSmsWorker *sms_worker)
{
    if (ui == NULL || runtime == NULL || member_login == NULL || sms_worker == NULL)
        return -1;
    memset(ui, 0, sizeof(*ui));
    ui->runtime = runtime;
    ui->member_login = member_login;
    ui->sms_worker = sms_worker;
    ui->background_pixels = calloc(LCD_WIDTH * LCD_HEIGHT, sizeof(*ui->background_pixels));
    ui->weather_icon_pixels = calloc(96 * 64, sizeof(*ui->weather_icon_pixels));
    if (ui->background_pixels == NULL || ui->weather_icon_pixels == NULL ||
        load_image_pixels(RESOURCE_DIR "/back.jpg", &ui->back_pixels,
                          BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT) == -1)
        goto failed;
    image_descriptor_init(&ui->background_dsc, ui->background_pixels,
                          LCD_WIDTH, LCD_HEIGHT);
    image_descriptor_init(&ui->back_dsc, ui->back_pixels,
                          BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT);
    image_descriptor_init(&ui->weather_icon_dsc, ui->weather_icon_pixels, 96, 64);
    ui->font_small = lv_freetype_font_create(MAIN_FONT_PATH,
                                             LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                             15, LV_FREETYPE_FONT_STYLE_NORMAL);
    ui->font_input = lv_freetype_font_create(MAIN_FONT_PATH,
                                             LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                             22, LV_FREETYPE_FONT_STYLE_NORMAL);
    if (ui->font_small == NULL || ui->font_input == NULL)
    {
        fprintf(stderr, "[LVGL] cannot create UI fonts from %s\n", MAIN_FONT_PATH);
        goto failed;
    }
    ui->screen = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(ui->screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui->screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui->screen, 0, 0);
    lv_obj_set_style_border_width(ui->screen, 0, 0);
    lv_obj_set_style_radius(ui->screen, 0, 0);
    ui->background = lv_image_create(ui->screen);
    lv_obj_set_pos(ui->background, 0, 0);
    lv_obj_remove_flag(ui->background, LV_OBJ_FLAG_CLICKABLE);
    ui->overlay = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(ui->overlay, 0, 0);
    lv_obj_set_style_bg_opa(ui->overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui->overlay, 0, 0);
    lv_obj_set_style_pad_all(ui->overlay, 0, 0);
    lv_obj_set_style_radius(ui->overlay, 0, 0);
    lv_obj_remove_flag(ui->overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(ui->screen);
    ui->dashboard_minute = -1;
    ui->requested_screen = -1;
    ui->initialized = 1;
    show_screen(ui, LVGL_UI_SCREEN_MAIN);
    return 0;

failed:
    lvgl_ui_destroy(ui);
    return -1;
}

void lvgl_ui_destroy(LvglUi *ui)
{
    if (ui == NULL)
        return;
    if (ui->screen != NULL)
        lv_obj_delete(ui->screen);
    if (ui->font_small != NULL)
        lv_freetype_font_delete(ui->font_small);
    if (ui->font_input != NULL)
        lv_freetype_font_delete(ui->font_input);
    free(ui->background_pixels);
    free(ui->back_pixels);
    free(ui->weather_icon_pixels);
    memset(ui, 0, sizeof(*ui));
}

void lvgl_ui_process(LvglUi *ui)
{
    if (ui == NULL || !ui->initialized)
        return;
    if (ui->requested_screen >= LVGL_UI_SCREEN_MAIN)
        show_screen(ui, ui->requested_screen);
    if (ui->screen_id == LVGL_UI_SCREEN_MAIN &&
        ((int)current_minute() != ui->dashboard_minute ||
         main_weather_version() != ui->dashboard_weather_version))
        draw_main_dashboard(ui);
    login_sync(ui);
    lvgl_runtime_process(ui->runtime);
}

void lvgl_ui_play_startup(LvglUi *ui)
{
    static const char *const frames[] = {
        RESOURCE_DIR "/logo/frame-0.jpeg", RESOURCE_DIR "/logo/frame-1.jpeg",
        RESOURCE_DIR "/logo/frame-2.jpeg", RESOURCE_DIR "/logo/frame-3.jpeg",
        RESOURCE_DIR "/logo/frame-4.jpeg"
    };

    if (ui == NULL || !ui->initialized)
        return;
    /* 动画阶段仅显示 JPEG 帧，避免主界面天气控件叠加到开机画面。 */
    lv_obj_add_flag(ui->overlay, LV_OBJ_FLAG_HIDDEN);
    for (int round = 0; round < 5; ++round)
    {
        for (size_t frame = 0; frame < 4; ++frame)
        {
            if (load_startup_background(ui, frames[frame]) == 0)
            {
                lv_obj_invalidate(ui->background);
                lv_refr_now(lvgl_runtime_display(ui->runtime));
            }
            usleep(100 * 1000);
        }
    }
    if (load_startup_background(ui, frames[4]) == 0)
    {
        lv_obj_invalidate(ui->background);
        lv_refr_now(lvgl_runtime_display(ui->runtime));
    }
    usleep(400 * 1000);
    lv_obj_clear_flag(ui->overlay, LV_OBJ_FLAG_HIDDEN);
    show_screen(ui, LVGL_UI_SCREEN_MAIN);
    lv_refr_now(lvgl_runtime_display(ui->runtime));
}

void lvgl_ui_show_main(LvglUi *ui)
{
    if (ui != NULL && ui->initialized)
    {
        lv_screen_load(ui->screen);
        show_screen(ui, LVGL_UI_SCREEN_MAIN);
    }
}

void lvgl_ui_show_login(LvglUi *ui)
{
    if (ui != NULL && ui->initialized)
    {
        lv_screen_load(ui->screen);
        member_login_reset(ui->member_login);
        show_screen(ui, LVGL_UI_SCREEN_LOGIN);
    }
}

LvglUiAction lvgl_ui_take_action(LvglUi *ui)
{
    LvglUiAction action;

    if (ui == NULL)
        return LVGL_UI_ACTION_NONE;
    action = ui->action;
    ui->action = LVGL_UI_ACTION_NONE;
    return action;
}
