#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "freetype_lcd.h"
#include "lcd.h"
#include "lvgl_runtime.h"
#include "lvgl_ui.h"
#include "main_weather.h"
#include "member_login.h"
#include "member_sms.h"
#include "touch.h"

static void press_key(LvglUi *ui, uint32_t button)
{
    lv_buttonmatrix_set_selected_button(ui->keyboard, button);
    assert(lv_obj_send_event(ui->keyboard, LV_EVENT_VALUE_CHANGED, NULL) ==
           LV_RESULT_OK);
}

int main(void)
{
    Lcd lcd;
    Touch touch;
    MemberLogin login;
    MemberSmsWorker sms_worker;
    LvglRuntime runtime;
    LvglUi ui;
    MemberSmsRequest sms_request;

    memset(&lcd, 0, sizeof(lcd));
    memset(&touch, 0, sizeof(touch));
    memset(&sms_worker, 0, sizeof(sms_worker));
    memset(&runtime, 0, sizeof(runtime));
    memset(&ui, 0, sizeof(ui));
    lcd.fd = -1;
    lcd.page_count = 1;
    lcd.base_pixels = calloc((size_t)LCD_WIDTH * LCD_HEIGHT,
                             sizeof(*lcd.base_pixels));
    assert(lcd.base_pixels != NULL);
    lcd.pixels = lcd.base_pixels;
    touch.fd = -1;

    assert(freetype_init(MAIN_FONT_PATH) == 0);
    assert(main_weather_init() == 0);
    assert(member_login_init(&login) == 0);
    assert(lvgl_runtime_init(&runtime, &lcd, &touch) == 0);
    assert(lvgl_ui_init(&ui, &runtime, &login, &sms_worker) == 0);

    lvgl_ui_show_login(&ui);
    assert(ui.active_input == ui.phone_input);
    assert(lv_obj_has_state(ui.phone_input, LV_STATE_FOCUSED));
    assert(!lv_obj_has_state(ui.code_input, LV_STATE_FOCUSED));

    press_key(&ui, 0); /* 1 */
    press_key(&ui, 1); /* 2 */
    press_key(&ui, 2); /* 3 */
    assert(strcmp(lv_textarea_get_text(ui.phone_input), "123") == 0);
    assert(ui.active_input == ui.phone_input);
    assert(lv_textarea_get_cursor_pos(ui.phone_input) == 3);

    assert(lv_obj_send_event(ui.code_input, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK);
    assert(ui.active_input == ui.code_input);
    assert(lv_obj_has_state(ui.code_input, LV_STATE_FOCUSED));
    assert(!lv_obj_has_state(ui.phone_input, LV_STATE_FOCUSED));
    press_key(&ui, 3); /* 4 */
    press_key(&ui, 4); /* 5 */
    assert(strcmp(lv_textarea_get_text(ui.code_input), "45") == 0);
    press_key(&ui, 9); /* backspace */
    assert(strcmp(lv_textarea_get_text(ui.code_input), "4") == 0);
    press_key(&ui, 11); /* submit */
    assert(ui.active_input == ui.code_input);
    assert(lv_textarea_get_cursor_pos(ui.code_input) == 1);

    assert(member_login_set_input(&login, MEMBER_LOGIN_FIELD_PHONE,
                                  "13800138000") == 0);
    assert(member_login_request_sms(&login, &sms_request) ==
           MEMBER_LOGIN_ACTION_SEND_SMS);
    lvgl_ui_process(&ui);
    assert(ui.active_input == ui.code_input);
    assert(lv_textarea_get_cursor_pos(ui.code_input) == 0);

    lvgl_ui_show_main(&ui);
    lvgl_ui_show_login(&ui);
    assert(ui.active_input == ui.phone_input);
    assert(lv_textarea_get_cursor_pos(ui.phone_input) == 0);

    lvgl_ui_show_main(&ui);
    {
        MainWeather updated = {
            .type = MAIN_WEATHER_HEAVY_RAIN,
            .temperature_c = 31
        };

        snprintf(updated.city, sizeof(updated.city), "%s", "广州");
        assert(main_weather_set(&updated) == 0);
        lvgl_ui_process(&ui);
        assert(strstr(lv_label_get_text(ui.weather_label), "广州") != NULL);
        assert(strstr(lv_label_get_text(ui.weather_label), "大雨") != NULL);
        assert(ui.weather_icon_pixels[0] != 0 ||
               ui.weather_icon_pixels[96 * 64 - 1] != 0);
    }

    lvgl_ui_destroy(&ui);
    lvgl_runtime_destroy(&runtime);
    member_login_destroy(&login);
    main_weather_destroy();
    freetype_exit();
    free(lcd.base_pixels);
    return 0;
}
