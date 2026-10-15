#ifndef MAIN_WEATHER_H
#define MAIN_WEATHER_H

#include "lcd.h"

#define MAIN_WEATHER_CITY_SIZE 16

typedef enum {
    MAIN_WEATHER_SUNNY = 0,
    MAIN_WEATHER_SUNNY_TO_CLOUDY,
    MAIN_WEATHER_CLOUDY,
    MAIN_WEATHER_OVERCAST,
    MAIN_WEATHER_LIGHT_RAIN,
    MAIN_WEATHER_MODERATE_RAIN,
    MAIN_WEATHER_HEAVY_RAIN,
    MAIN_WEATHER_THUNDERSTORM,
    MAIN_WEATHER_LIGHT_SNOW,
    MAIN_WEATHER_HEAVY_SNOW,
    MAIN_WEATHER_FOG,
    MAIN_WEATHER_HAZE,
    MAIN_WEATHER_COUNT
} MainWeatherType;

typedef struct {
    MainWeatherType type;
    int temperature_c;
    char city[MAIN_WEATHER_CITY_SIZE];
} MainWeather;

/* Load and cache all weather assets. Call before using the other functions. */
int main_weather_init(void);
void main_weather_destroy(void);

/* Thread-safe state update and snapshot functions. */
int main_weather_set(const MainWeather *weather);
int main_weather_get(MainWeather *weather, unsigned long *version);
unsigned long main_weather_version(void);

/* Return the Chinese label used beside the temperature. */
const char *main_weather_label(MainWeatherType type);

/* Draw the cached, background-matched icon into every framebuffer page. */
int main_weather_draw_icon(Lcd *lcd, const MainWeather *weather);
int main_weather_draw_icon_pixels(int *pixels, const MainWeather *weather);
int main_weather_copy_icon_pixels(int *pixels, const MainWeather *weather);

#endif
