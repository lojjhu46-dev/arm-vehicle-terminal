#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "main_weather.h"

#define TEST_PAGE_PIXELS (LCD_WIDTH * LCD_HEIGHT)

static int framebuffer[LCD_MAX_PAGES * TEST_PAGE_PIXELS];

int main(void)
{
    Lcd lcd;
    MainWeather weather;
    MainWeather updated = {
        .type = MAIN_WEATHER_HAZE,
        .temperature_c = -6,
        .city = ""
    };
    unsigned long first_version;
    unsigned long second_version;

    snprintf(updated.city, sizeof(updated.city), "%s", "广州");
    if (main_weather_init() != 0 ||
        main_weather_get(&weather, &first_version) != 0 ||
        weather.type != MAIN_WEATHER_SUNNY_TO_CLOUDY ||
        weather.temperature_c != 23 ||
        strcmp(weather.city, "深圳") != 0 ||
        strcmp(main_weather_label(weather.type), "晴转多云") != 0)
        return 1;

    if (main_weather_set(&updated) != 0 ||
        main_weather_get(&weather, &second_version) != 0 ||
        second_version <= first_version ||
        weather.type != MAIN_WEATHER_HAZE ||
        weather.temperature_c != -6 ||
        strcmp(weather.city, "广州") != 0 ||
        strcmp(main_weather_label(weather.type), "霾") != 0)
        return 1;

    memset(framebuffer, 0, sizeof(framebuffer));
    memset(&lcd, 0, sizeof(lcd));
    lcd.base_pixels = framebuffer;
    lcd.page_count = LCD_MAX_PAGES;
    if (main_weather_draw_icon(&lcd, &weather) != 0 ||
        memcmp(framebuffer,
               framebuffer + TEST_PAGE_PIXELS,
               TEST_PAGE_PIXELS * sizeof(*framebuffer)) != 0 ||
        memcmp(framebuffer,
               framebuffer + 2 * TEST_PAGE_PIXELS,
               TEST_PAGE_PIXELS * sizeof(*framebuffer)) != 0)
        return 1;

    main_weather_destroy();
    return 0;
}
