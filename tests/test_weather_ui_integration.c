#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "lcd.h"
#include "lvgl_runtime.h"
#include "lvgl_ui.h"
#include "main_weather.h"
#include "member_login.h"
#include "member_sms.h"
#include "weather_receiver.h"

/* 构造与 README 一致的 weather.hourly.v1 报文：24 条 +08:00 小时数据。 */
static void build_forecast_payload(char *json, size_t json_size,
                                   const char *city, const char *text,
                                   double temp_c)
{
    size_t used = 0;
    time_t base = time(NULL);

    base -= base % 3600;
    used += (size_t)snprintf(json + used, json_size - used,
                             "{\"type\":\"weather.hourly.v1\",\"city\":\"%s\","
                             "\"latitude\":23.1291,\"longitude\":113.2644,"
                             "\"hours\":[", city);
    for (int i = 0; i < 24; ++i)
    {
        char hour[WEATHER_FORECAST_TIME_SIZE];
        struct tm utc_plus_eight;
        time_t hour_time = base + (time_t)i * 3600 + 8 * 60 * 60;
        int written;

        assert(gmtime_r(&hour_time, &utc_plus_eight) != NULL);
        assert(strftime(hour, sizeof(hour), "%Y-%m-%dT%H:00+08:00",
                        &utc_plus_eight) == 22);
        written = snprintf(json + used, json_size - used,
                           "%s{\"time\":\"%s\",\"text\":\"%s\",\"temp_c\":%.1f}",
                           i == 0 ? "" : ",", hour,
                           i == 0 ? text : "晴", temp_c + i);
        assert(written > 0 && (size_t)written < json_size - used);
        used += (size_t)written;
    }
    assert(used + 3 < json_size);
    memcpy(json + used, "]}", 3);
}

/* 模拟 TCP 接收线程：解析报文后调用 main_weather_set 更新天气状态。 */
static void *receiver_thread_main(void *argument)
{
    char *json = argument;
    WeatherForecast forecast;
    MainWeather weather;
    size_t city_length;

    assert(weather_receiver_parse_hourly_json(json, &forecast) == 0);
    memset(&weather, 0, sizeof(weather));
    weather.type = MAIN_WEATHER_MODERATE_RAIN;
    weather.temperature_c = 27;
    city_length = strlen(forecast.city);
    assert(city_length < sizeof(weather.city));
    memcpy(weather.city, forecast.city, city_length + 1);
    assert(main_weather_set(&weather) == 0);
    return NULL;
}

static unsigned long framebuffer_checksum(const int *pixels)
{
    unsigned long sum = 0;

    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i)
        sum = sum * 31 + (unsigned int)pixels[i];
    return sum;
}

int main(void)
{
    Lcd lcd;
    Touch touch;
    MemberLogin login;
    MemberSmsWorker sms_worker;
    LvglRuntime runtime;
    LvglUi ui;
    char json[4096];
    pthread_t receiver_thread;
    unsigned long before;
    unsigned long after;

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
    lvgl_ui_process(&ui);
    before = framebuffer_checksum(lcd.base_pixels);

    build_forecast_payload(json, sizeof(json), "广州", "中雨", 27.0);
    assert(pthread_create(&receiver_thread, NULL, receiver_thread_main,
                          json) == 0);
    pthread_join(receiver_thread, NULL);

    lvgl_ui_process(&ui);
    assert(strstr(lv_label_get_text(ui.weather_label), "广州") != NULL);
    assert(strstr(lv_label_get_text(ui.weather_label), "中雨") != NULL);
    after = framebuffer_checksum(lcd.base_pixels);
    assert(before != after);

    lvgl_ui_destroy(&ui);
    lvgl_runtime_destroy(&runtime);
    member_login_destroy(&login);
    main_weather_destroy();
    freetype_exit();
    free(lcd.base_pixels);
    return 0;
}
