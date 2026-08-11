#ifndef WEATHER_RECEIVER_H
#define WEATHER_RECEIVER_H

#include <stddef.h>

#define WEATHER_HOURLY_PROTOCOL_COUNT 24
#define WEATHER_FORECAST_CITY_SIZE 32
#define WEATHER_FORECAST_TIME_SIZE 26
#define WEATHER_FORECAST_TEXT_SIZE 32

typedef struct {
    char city[WEATHER_FORECAST_CITY_SIZE];
    char time[WEATHER_FORECAST_TIME_SIZE];
    char text[WEATHER_FORECAST_TEXT_SIZE];
    double temp_c;
} WeatherForecast;

typedef int (*WeatherTextHandler)(const char *message, char *reply,
                                  size_t reply_size, void *user_data);

/* Parse and select one current weather snapshot from a weather.hourly.v1 payload. */
int weather_receiver_parse_hourly_json(const char *json, WeatherForecast *forecast);

/* Start/stop the TCP receiver. start is idempotent; stop joins its thread. */
int weather_receiver_start(void);
void weather_receiver_stop(void);

/* Optional handler for non-JSON AI reply text received on the same port. */
void weather_receiver_set_text_handler(WeatherTextHandler handler, void *user_data);

#endif
