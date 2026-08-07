#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "main_weather.h"
#include "ui_remote_commands.h"
#include "weather_receiver.h"

static pthread_mutex_t weather_mutex = PTHREAD_MUTEX_INITIALIZER;
static MainWeather last_weather;
static int weather_update_count;

int main_weather_set(const MainWeather *weather)
{
    pthread_mutex_lock(&weather_mutex);
    last_weather = *weather;
    ++weather_update_count;
    pthread_mutex_unlock(&weather_mutex);
    return 0;
}

static int build_forecast(char *json, size_t json_size, int first_hour_offset)
{
    size_t used = 0;
    time_t base = time(NULL);

    base -= base % 3600;
    used += (size_t)snprintf(json + used, json_size - used,
                             "{\"type\":\"weather.hourly.v1\",\"city\":\"广州\","
                             "\"latitude\":23.1291,\"longitude\":113.2644,\"hours\":[");
    for (int i = 0; i < WEATHER_HOUR_COUNT; ++i)
    {
        char hour[WEATHER_FORECAST_TIME_SIZE];
        struct tm utc_plus_eight;
        time_t hour_time = base + (first_hour_offset + i) * 3600 + 8 * 60 * 60;
        int written;

        assert(gmtime_r(&hour_time, &utc_plus_eight) != NULL);
        assert(strftime(hour, sizeof(hour), "%Y-%m-%dT%H:00+08:00",
                        &utc_plus_eight) == 22);
        written = snprintf(json + used, json_size - used,
                           "%s{\"time\":\"%s\",\"text\":\"%s\",\"temp_c\":%.1f}",
                           i == 0 ? "" : ",", hour,
                           i == 0 ? "小雨" : "晴",
                           26.4 + first_hour_offset + i);
        assert(written > 0 && (size_t)written < json_size - used);
        used += (size_t)written;
    }
    assert(used + 3 < json_size);
    memcpy(json + used, "]}", 3);
    return 0;
}

static int send_request(const char *payload, char *reply, size_t reply_size)
{
    struct sockaddr_in address;
    size_t sent = 0;
    size_t received_total = 0;
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    assert(fd >= 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(WEATHER_RECEIVER_PORT);
    assert(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
    assert(connect(fd, (const struct sockaddr *)&address, sizeof(address)) == 0);
    while (sent < strlen(payload))
    {
        ssize_t written = send(fd, payload + sent, strlen(payload) - sent, 0);

        assert(written > 0);
        sent += (size_t)written;
    }
    assert(shutdown(fd, SHUT_WR) == 0);
    while (received_total + 1 < reply_size)
    {
        ssize_t received = recv(fd, reply + received_total,
                                reply_size - received_total - 1, 0);

        assert(received >= 0);
        if (received == 0)
            break;
        received_total += (size_t)received;
    }
    reply[received_total] = '\0';
    close(fd);
    return 0;
}

int main(void)
{
    char json[4096];
    char future_json[4096];
    char past_json[4096];
    char reply[128];
    WeatherForecast forecast;
    UiRemoteCommands commands;
    UiRemoteCommandRequest command_request;

    build_forecast(json, sizeof(json), 0);
    assert(weather_receiver_parse_hourly_json(json, &forecast) == 0);
    assert(strcmp(forecast.city, "广州") == 0);
    assert(strcmp(forecast.hours[0].text, "小雨") == 0);
    assert(weather_receiver_parse_hourly_json(
               "{\"type\":\"weather.hourly.v1\",\"city\":\"广州\","
               "\"latitude\":23.1291,\"longitude\":113.2644,\"hours\":[]}",
               &forecast) == -1);

    assert(ui_remote_commands_init(&commands) == 0);
    weather_receiver_set_text_handler(ui_remote_commands_handle_text, &commands);
    if (weather_receiver_start() != 0)
    {
        weather_receiver_set_text_handler(NULL, NULL);
        ui_remote_commands_destroy(&commands);
        if (errno == EPERM || errno == EACCES)
            return 0; /* The sandbox does not allow binding a TCP listener. */
        perror("weather_receiver_start");
        return 1;
    }
    assert(send_request("开始录像", reply, sizeof(reply)) == 0);
    assert(strcmp(reply, "OK command accepted\n") == 0);
    ui_remote_commands_take(&commands, &command_request);
    assert(command_request.recording_target == 1);
    assert(command_request.parking_guides_target == -1);
    assert(command_request.take_photo == 0);
    assert(send_request("好的，正在为您开启摄像头。", reply, sizeof(reply)) == 0);
    assert(strcmp(reply, "OK text received\n") == 0);
    assert(send_request(json, reply, sizeof(reply)) == 0);
    assert(strcmp(reply, "OK weather cached\n") == 0);

    pthread_mutex_lock(&weather_mutex);
    assert(weather_update_count == 1);
    assert(last_weather.type == MAIN_WEATHER_LIGHT_RAIN);
    assert(last_weather.temperature_c == 26);
    assert(strcmp(last_weather.city, "广州") == 0);
    pthread_mutex_unlock(&weather_mutex);

    build_forecast(future_json, sizeof(future_json), 1);
    assert(send_request(future_json, reply, sizeof(reply)) == 0);
    assert(strcmp(reply, "OK weather cached\n") == 0);
    pthread_mutex_lock(&weather_mutex);
    assert(weather_update_count == 2);
    assert(last_weather.temperature_c == 27);
    pthread_mutex_unlock(&weather_mutex);

    build_forecast(past_json, sizeof(past_json), -24);
    assert(send_request(past_json, reply, sizeof(reply)) == 0);
    assert(strcmp(reply, "OK weather cached\n") == 0);
    pthread_mutex_lock(&weather_mutex);
    assert(weather_update_count == 3);
    assert(last_weather.temperature_c == 25);
    pthread_mutex_unlock(&weather_mutex);
    weather_receiver_stop();
    weather_receiver_set_text_handler(NULL, NULL);
    ui_remote_commands_destroy(&commands);
    return 0;
}
