#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "main_weather.h"

#define MAIN_WEATHER_ICON_X 352
#define MAIN_WEATHER_ICON_Y 68
#define MAIN_WEATHER_ICON_W 96
#define MAIN_WEATHER_ICON_H 64

typedef struct {
    pthread_mutex_t mutex;
    int initialized;
    MainWeather current;
    unsigned long version;
    int *icons[MAIN_WEATHER_COUNT];
} MainWeatherContext;

static MainWeatherContext context;

static const char *const weather_names[MAIN_WEATHER_COUNT] = {
    "sunny",
    "sunny_to_cloudy",
    "cloudy",
    "overcast",
    "light_rain",
    "moderate_rain",
    "heavy_rain",
    "thunderstorm",
    "light_snow",
    "heavy_snow",
    "fog",
    "haze"
};

static const char *const weather_labels[MAIN_WEATHER_COUNT] = {
    "晴天",
    "晴转多云",
    "多云",
    "阴天",
    "小雨",
    "中雨",
    "大雨",
    "雷阵雨",
    "小雪",
    "大雪",
    "雾",
    "霾"
};

// 释放已加载的全部天气图标像素缓冲。
static void free_icons(void)
{
    for (int i = 0; i < MAIN_WEATHER_COUNT; ++i)
    {
        free(context.icons[i]);
        context.icons[i] = NULL;
    }
}

// 加载天气图标并建立默认天气快照。
int main_weather_init(void)
{
    if (context.initialized)
        return 0;
    if (pthread_mutex_init(&context.mutex, NULL) != 0)
        return -1;

    for (int i = 0; i < MAIN_WEATHER_COUNT; ++i)
    {
        char path[256];
        int width;
        int height;

        if (snprintf(path, sizeof(path), "%s/%s.jpeg",
                     MAIN_WEATHER_ICON_DIR, weather_names[i]) >=
            (int)sizeof(path) ||
            read_JPEG_image(path, &context.icons[i], &width, &height) == -1 ||
            width != MAIN_WEATHER_ICON_W || height != MAIN_WEATHER_ICON_H)
        {
            fprintf(stderr, "invalid weather asset: %s\n", path);
            free_icons();
            pthread_mutex_destroy(&context.mutex);
            return -1;
        }
    }

    memset(&context.current, 0, sizeof(context.current));
    context.current.type = MAIN_WEATHER_SUNNY_TO_CLOUDY;
    context.current.temperature_c = 23;
    snprintf(context.current.city, sizeof(context.current.city), "%s", "深圳");
    context.version = 1;
    context.initialized = 1;
    return 0;
}

// 销毁天气缓存和其互斥锁。
void main_weather_destroy(void)
{
    if (!context.initialized)
        return;
    free_icons();
    context.initialized = 0;
    pthread_mutex_destroy(&context.mutex);
}

// 校验并原子替换当前天气，递增版本号通知界面刷新。
int main_weather_set(const MainWeather *weather)
{
    size_t city_length;

    if (!context.initialized || weather == NULL ||
        weather->type < 0 || weather->type >= MAIN_WEATHER_COUNT ||
        weather->temperature_c < -99 || weather->temperature_c > 99)
        return -1;
    city_length = strnlen(weather->city, sizeof(weather->city));
    if (city_length == 0 || city_length >= sizeof(weather->city))
        return -1;

    pthread_mutex_lock(&context.mutex);
    context.current = *weather;
    context.current.city[sizeof(context.current.city) - 1] = '\0';
    ++context.version;
    pthread_mutex_unlock(&context.mutex);
    return 0;
}

// 读取一份一致的天气快照及可选版本号。
int main_weather_get(MainWeather *weather, unsigned long *version)
{
    if (!context.initialized || weather == NULL)
        return -1;

    pthread_mutex_lock(&context.mutex);
    *weather = context.current;
    if (version != NULL)
        *version = context.version;
    pthread_mutex_unlock(&context.mutex);
    return 0;
}

// 获取天气版本号，供主界面判断是否需要重绘。
unsigned long main_weather_version(void)
{
    unsigned long version = 0;

    if (!context.initialized)
        return version;
    pthread_mutex_lock(&context.mutex);
    version = context.version;
    pthread_mutex_unlock(&context.mutex);
    return version;
}

// 将天气枚举转换为用户可见的中文标签。
const char *main_weather_label(MainWeatherType type)
{
    if (type < 0 || type >= MAIN_WEATHER_COUNT)
        return "未知";
    return weather_labels[type];
}

// 将当前天气图标复制到所有 framebuffer 页面，避免翻页后图标丢失。
int main_weather_draw_icon(Lcd *lcd, const MainWeather *weather)
{
    int *icon;

    if (!context.initialized || lcd == NULL || weather == NULL ||
        weather->type < 0 || weather->type >= MAIN_WEATHER_COUNT)
        return -1;

    pthread_mutex_lock(&context.mutex);
    icon = context.icons[weather->type];
    pthread_mutex_unlock(&context.mutex);

    for (int page = 0; page < lcd_page_count(lcd); ++page)
    {
        int *destination = lcd_page_pixels(lcd, page);
        for (int y = 0; y < MAIN_WEATHER_ICON_H; ++y)
        {
            memcpy(destination + (MAIN_WEATHER_ICON_Y + y) * LCD_WIDTH +
                       MAIN_WEATHER_ICON_X,
                   icon + y * MAIN_WEATHER_ICON_W,
                   MAIN_WEATHER_ICON_W * sizeof(*destination));
        }
    }
    return 0;
}
