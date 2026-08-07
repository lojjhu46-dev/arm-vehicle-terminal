#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "main_weather.h"
#include "weather_receiver.h"

#define WEATHER_JSON_MAX_DEPTH 8
#define WEATHER_REPLY_SIZE 128

typedef struct {
    const char *position;
    const char *end;
} JsonCursor;

typedef struct {
    pthread_mutex_t mutex;
    pthread_t thread;
    int thread_started;
    int running;
    int listen_fd;
    int has_forecast;
    unsigned long forecast_version;
    unsigned long applied_version;
    char applied_hour[14];
    WeatherForecast forecast;
    WeatherTextHandler text_handler;
    void *text_handler_data;
} WeatherReceiverContext;

static WeatherReceiverContext receiver = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .listen_fd = -1
};

static void json_skip_whitespace(JsonCursor *cursor)
{
    while (cursor->position < cursor->end &&
           isspace((unsigned char)*cursor->position))
        ++cursor->position;
}

static int json_expect(JsonCursor *cursor, char expected)
{
    json_skip_whitespace(cursor);
    if (cursor->position >= cursor->end || *cursor->position != expected)
        return -1;
    ++cursor->position;
    return 0;
}

static int json_parse_string(JsonCursor *cursor, char *output, size_t output_size)
{
    size_t length = 0;

    json_skip_whitespace(cursor);
    if (cursor->position >= cursor->end || *cursor->position != '"')
        return -1;
    ++cursor->position;
    while (cursor->position < cursor->end)
    {
        char character = *cursor->position++;

        if (character == '"')
        {
            if (output != NULL)
                output[length] = '\0';
            return 0;
        }
        if ((unsigned char)character < 0x20)
            return -1;
        if (character == '\\')
        {
            if (cursor->position >= cursor->end)
                return -1;
            character = *cursor->position++;
            switch (character)
            {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                character = '\b';
                break;
            case 'f':
                character = '\f';
                break;
            case 'n':
                character = '\n';
                break;
            case 'r':
                character = '\r';
                break;
            case 't':
                character = '\t';
                break;
            default:
                /* The protocol uses UTF-8 directly; reject unsupported escapes. */
                return -1;
            }
        }
        if (output != NULL)
        {
            if (length + 1 >= output_size)
                return -1;
            output[length] = character;
        }
        ++length;
    }
    return -1;
}

static int json_parse_number(JsonCursor *cursor, double *value)
{
    char *end_pointer;

    json_skip_whitespace(cursor);
    if (cursor->position >= cursor->end)
        return -1;
    errno = 0;
    *value = strtod(cursor->position, &end_pointer);
    if (end_pointer == cursor->position || end_pointer > cursor->end ||
        errno == ERANGE || !isfinite(*value))
        return -1;
    cursor->position = end_pointer;
    return 0;
}

static int json_parse_literal(JsonCursor *cursor, const char *literal)
{
    size_t length = strlen(literal);

    json_skip_whitespace(cursor);
    if ((size_t)(cursor->end - cursor->position) < length ||
        strncmp(cursor->position, literal, length) != 0)
        return -1;
    cursor->position += length;
    return 0;
}

static int json_skip_value(JsonCursor *cursor, int depth)
{
    double ignored_number;
    int first = 1;

    if (depth > WEATHER_JSON_MAX_DEPTH)
        return -1;
    json_skip_whitespace(cursor);
    if (cursor->position >= cursor->end)
        return -1;
    switch (*cursor->position)
    {
    case '"':
        return json_parse_string(cursor, NULL, 0);
    case '{':
        ++cursor->position;
        for (;;)
        {
            json_skip_whitespace(cursor);
            if (cursor->position < cursor->end && *cursor->position == '}')
            {
                ++cursor->position;
                return 0;
            }
            if (!first && json_expect(cursor, ',') == -1)
                return -1;
            first = 0;
            if (json_parse_string(cursor, NULL, 0) == -1 ||
                json_expect(cursor, ':') == -1 ||
                json_skip_value(cursor, depth + 1) == -1)
                return -1;
        }
    case '[':
        ++cursor->position;
        for (;;)
        {
            json_skip_whitespace(cursor);
            if (cursor->position < cursor->end && *cursor->position == ']')
            {
                ++cursor->position;
                return 0;
            }
            if (!first && json_expect(cursor, ',') == -1)
                return -1;
            first = 0;
            if (json_skip_value(cursor, depth + 1) == -1)
                return -1;
        }
    case 't':
        return json_parse_literal(cursor, "true");
    case 'f':
        return json_parse_literal(cursor, "false");
    case 'n':
        return json_parse_literal(cursor, "null");
    default:
        return json_parse_number(cursor, &ignored_number);
    }
}

static int digits_at(const char *text, size_t offset, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        if (!isdigit((unsigned char)text[offset + i]))
            return 0;
    }
    return 1;
}

static int valid_hour_time(const char *text)
{
    int month;
    int day;
    int hour;
    int minute;

    if (strlen(text) != 22 || text[4] != '-' || text[7] != '-' ||
        text[10] != 'T' || text[13] != ':' ||
        text[16] != '+' || text[17] != '0' || text[18] != '8' ||
        text[19] != ':' || text[20] != '0' || text[21] != '0' ||
        !digits_at(text, 0, 4) || !digits_at(text, 5, 2) ||
        !digits_at(text, 8, 2) || !digits_at(text, 11, 2) ||
        !digits_at(text, 14, 2) || !digits_at(text, 17, 2) ||
        !digits_at(text, 20, 2))
        return 0;
    month = (text[5] - '0') * 10 + text[6] - '0';
    day = (text[8] - '0') * 10 + text[9] - '0';
    hour = (text[11] - '0') * 10 + text[12] - '0';
    minute = (text[14] - '0') * 10 + text[15] - '0';
    return month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
           hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

static int parse_hourly_object(JsonCursor *cursor, WeatherHourly *hour)
{
    char key[16];
    int first = 1;
    int has_time = 0;
    int has_text = 0;
    int has_temperature = 0;

    memset(hour, 0, sizeof(*hour));
    if (json_expect(cursor, '{') == -1)
        return -1;
    for (;;)
    {
        json_skip_whitespace(cursor);
        if (cursor->position < cursor->end && *cursor->position == '}')
        {
            ++cursor->position;
            break;
        }
        if (!first && json_expect(cursor, ',') == -1)
            return -1;
        first = 0;
        if (json_parse_string(cursor, key, sizeof(key)) == -1 ||
            json_expect(cursor, ':') == -1)
            return -1;
        if (strcmp(key, "time") == 0)
        {
            if (has_time || json_parse_string(cursor, hour->time,
                                               sizeof(hour->time)) == -1)
                return -1;
            has_time = 1;
        }
        else if (strcmp(key, "text") == 0)
        {
            if (has_text || json_parse_string(cursor, hour->text,
                                               sizeof(hour->text)) == -1)
                return -1;
            has_text = 1;
        }
        else if (strcmp(key, "temp_c") == 0)
        {
            if (has_temperature || json_parse_number(cursor, &hour->temp_c) == -1)
                return -1;
            has_temperature = 1;
        }
        else if (json_skip_value(cursor, 0) == -1)
            return -1;
    }
    if (!has_time || !has_text || !has_temperature || !valid_hour_time(hour->time) ||
        hour->text[0] == '\0' || hour->temp_c < -99.0 || hour->temp_c > 99.0)
        return -1;
    return 0;
}

static int parse_hours(JsonCursor *cursor, WeatherForecast *forecast)
{
    int count = 0;
    int first = 1;

    if (json_expect(cursor, '[') == -1)
        return -1;
    for (;;)
    {
        json_skip_whitespace(cursor);
        if (cursor->position < cursor->end && *cursor->position == ']')
        {
            ++cursor->position;
            break;
        }
        if (!first && json_expect(cursor, ',') == -1)
            return -1;
        first = 0;
        if (count >= WEATHER_HOUR_COUNT ||
            parse_hourly_object(cursor, &forecast->hours[count]) == -1)
            return -1;
        ++count;
    }
    return count == WEATHER_HOUR_COUNT ? 0 : -1;
}

int weather_receiver_parse_hourly_json(const char *json, WeatherForecast *forecast)
{
    JsonCursor cursor;
    char key[16];
    char type[32];
    int first = 1;
    int has_type = 0;
    int has_city = 0;
    int has_latitude = 0;
    int has_longitude = 0;
    int has_hours = 0;

    if (json == NULL || forecast == NULL ||
        strnlen(json, WEATHER_RECEIVER_MAX_PAYLOAD + 1) >
            WEATHER_RECEIVER_MAX_PAYLOAD)
        return -1;
    memset(forecast, 0, sizeof(*forecast));
    cursor.position = json;
    cursor.end = json + strlen(json);
    if (json_expect(&cursor, '{') == -1)
        return -1;
    for (;;)
    {
        json_skip_whitespace(&cursor);
        if (cursor.position < cursor.end && *cursor.position == '}')
        {
            ++cursor.position;
            break;
        }
        if (!first && json_expect(&cursor, ',') == -1)
            return -1;
        first = 0;
        if (json_parse_string(&cursor, key, sizeof(key)) == -1 ||
            json_expect(&cursor, ':') == -1)
            return -1;
        if (strcmp(key, "type") == 0)
        {
            if (has_type || json_parse_string(&cursor, type, sizeof(type)) == -1)
                return -1;
            has_type = 1;
        }
        else if (strcmp(key, "city") == 0)
        {
            if (has_city || json_parse_string(&cursor, forecast->city,
                                               sizeof(forecast->city)) == -1)
                return -1;
            has_city = 1;
        }
        else if (strcmp(key, "latitude") == 0)
        {
            if (has_latitude || json_parse_number(&cursor, &forecast->latitude) == -1)
                return -1;
            has_latitude = 1;
        }
        else if (strcmp(key, "longitude") == 0)
        {
            if (has_longitude || json_parse_number(&cursor, &forecast->longitude) == -1)
                return -1;
            has_longitude = 1;
        }
        else if (strcmp(key, "hours") == 0)
        {
            if (has_hours || parse_hours(&cursor, forecast) == -1)
                return -1;
            has_hours = 1;
        }
        else if (json_skip_value(&cursor, 0) == -1)
            return -1;
    }
    json_skip_whitespace(&cursor);
    if (cursor.position != cursor.end || !has_type || !has_city ||
        !has_latitude || !has_longitude || !has_hours ||
        forecast->city[0] == '\0' ||
        strnlen(forecast->city, sizeof(forecast->city)) >=
            MAIN_WEATHER_CITY_SIZE ||
        forecast->latitude < -90.0 || forecast->latitude > 90.0 ||
        forecast->longitude < -180.0 || forecast->longitude > 180.0 ||
        strcmp(type, "weather.hourly.v1") != 0)
        return -1;
    return 0;
}

static MainWeatherType weather_type_from_text(const char *text)
{
    if (strstr(text, "雷") != NULL)
        return MAIN_WEATHER_THUNDERSTORM;
    if (strstr(text, "雾") != NULL)
        return MAIN_WEATHER_FOG;
    if (strstr(text, "霾") != NULL)
        return MAIN_WEATHER_HAZE;
    if (strstr(text, "雪") != NULL)
        return strstr(text, "小雪") != NULL ? MAIN_WEATHER_LIGHT_SNOW :
                                               MAIN_WEATHER_HEAVY_SNOW;
    if (strstr(text, "雨") != NULL)
    {
        if (strstr(text, "暴雨") != NULL || strstr(text, "大雨") != NULL)
            return MAIN_WEATHER_HEAVY_RAIN;
        if (strstr(text, "中雨") != NULL)
            return MAIN_WEATHER_MODERATE_RAIN;
        return MAIN_WEATHER_LIGHT_RAIN;
    }
    if (strstr(text, "少云") != NULL || strstr(text, "晴") != NULL)
        return strstr(text, "多云") != NULL || strstr(text, "少云") != NULL ?
                   MAIN_WEATHER_SUNNY_TO_CLOUDY : MAIN_WEATHER_SUNNY;
    if (strstr(text, "多云") != NULL)
        return MAIN_WEATHER_CLOUDY;
    if (strstr(text, "阴") != NULL)
        return MAIN_WEATHER_OVERCAST;
    return MAIN_WEATHER_CLOUDY;
}

static int current_hour_key(char key[14])
{
    time_t now = time(NULL);
    struct tm utc_plus_eight;

    if (now == (time_t)-1)
        return -1;
    now += 8 * 60 * 60;
    if (gmtime_r(&now, &utc_plus_eight) == NULL)
        return -1;
    return strftime(key, 14, "%Y-%m-%dT%H", &utc_plus_eight) == 13 ? 0 : -1;
}

static int forecast_hour_index(const WeatherForecast *forecast, const char hour[14])
{
    int exact_index = -1;
    int future_index = -1;
    int latest_index = -1;

    for (int i = 0; i < WEATHER_HOUR_COUNT; ++i)
    {
        if (strncmp(forecast->hours[i].time, hour, 13) == 0)
            exact_index = i;
        if (strncmp(forecast->hours[i].time, hour, 13) > 0 &&
            (future_index < 0 ||
             strcmp(forecast->hours[i].time,
                    forecast->hours[future_index].time) < 0))
            future_index = i;
        if (latest_index < 0 ||
            strcmp(forecast->hours[i].time,
                   forecast->hours[latest_index].time) > 0)
            latest_index = i;
    }
    if (exact_index >= 0)
        return exact_index;
    if (future_index >= 0)
        return future_index;
    return latest_index;
}

static void weather_receiver_apply_current_hour(void)
{
    WeatherForecast forecast;
    MainWeather weather;
    char hour[14];
    unsigned long version;
    size_t city_length;
    int index;

    if (current_hour_key(hour) == -1)
        return;
    pthread_mutex_lock(&receiver.mutex);
    if (!receiver.has_forecast ||
        (receiver.applied_version == receiver.forecast_version &&
         strcmp(receiver.applied_hour, hour) == 0))
    {
        pthread_mutex_unlock(&receiver.mutex);
        return;
    }
    forecast = receiver.forecast;
    version = receiver.forecast_version;
    pthread_mutex_unlock(&receiver.mutex);

    index = forecast_hour_index(&forecast, hour);
    if (index < 0)
    {
        fprintf(stderr, "[Weather] no usable hour in cached forecast for %s\n",
                hour);
        return;
    }
    fprintf(stderr, "[Weather] applying hour=%s text=%s temp=%.1f city=%s\n",
            forecast.hours[index].time, forecast.hours[index].text,
            forecast.hours[index].temp_c, forecast.city);
    memset(&weather, 0, sizeof(weather));
    weather.type = weather_type_from_text(forecast.hours[index].text);
    weather.temperature_c = (int)lround(forecast.hours[index].temp_c);
    city_length = strlen(forecast.city);
    if (city_length >= sizeof(weather.city))
        return;
    memcpy(weather.city, forecast.city, city_length + 1);
    if (main_weather_set(&weather) == -1)
    {
        fprintf(stderr, "[Weather] main weather update failed\n");
        return;
    }

    pthread_mutex_lock(&receiver.mutex);
    if (receiver.forecast_version == version)
    {
        receiver.applied_version = version;
        memcpy(receiver.applied_hour, hour, sizeof(receiver.applied_hour));
    }
    pthread_mutex_unlock(&receiver.mutex);
}

static void weather_receiver_store_forecast(const WeatherForecast *forecast)
{
    pthread_mutex_lock(&receiver.mutex);
    receiver.forecast = *forecast;
    receiver.has_forecast = 1;
    ++receiver.forecast_version;
    receiver.applied_version = 0;
    receiver.applied_hour[0] = '\0';
    pthread_mutex_unlock(&receiver.mutex);
    weather_receiver_apply_current_hour();
}

static int send_reply(int client_fd, const char *reply)
{
    size_t sent = 0;
    size_t length = strlen(reply);

    while (sent < length)
    {
        ssize_t written = send(client_fd, reply + sent, length - sent,
#ifdef MSG_NOSIGNAL
                               MSG_NOSIGNAL
#else
                               0
#endif
        );

        if (written > 0)
        {
            sent += (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        return -1;
    }
    return 0;
}

static int receive_payload(int client_fd, char payload[WEATHER_RECEIVER_MAX_PAYLOAD + 1])
{
    size_t length = 0;

    for (;;)
    {
        char chunk[1024];
        ssize_t received = recv(client_fd, chunk, sizeof(chunk), 0);

        if (received > 0)
        {
            if ((size_t)received > WEATHER_RECEIVER_MAX_PAYLOAD - length)
                return -1;
            if (memchr(chunk, '\0', (size_t)received) != NULL)
                return -1;
            memcpy(payload + length, chunk, (size_t)received);
            length += (size_t)received;
            continue;
        }
        if (received == 0)
        {
            payload[length] = '\0';
            return length == 0 ? -1 : 0;
        }
        if (errno == EINTR)
            continue;
        return -1;
    }
}

static void handle_text_payload(const char *payload, char reply[WEATHER_REPLY_SIZE])
{
    WeatherTextHandler handler;
    void *handler_data;

    pthread_mutex_lock(&receiver.mutex);
    handler = receiver.text_handler;
    handler_data = receiver.text_handler_data;
    pthread_mutex_unlock(&receiver.mutex);
    if (handler == NULL)
    {
        fprintf(stderr, "AI text received: %s\n", payload);
        snprintf(reply, WEATHER_REPLY_SIZE, "%s", "OK text received\n");
        return;
    }
    if (handler(payload, reply, WEATHER_REPLY_SIZE, handler_data) != 0 ||
        reply[0] == '\0')
        snprintf(reply, WEATHER_REPLY_SIZE, "%s", "ERROR text handler failed\n");
}

static void handle_client(int client_fd)
{
    char payload[WEATHER_RECEIVER_MAX_PAYLOAD + 1];
    char reply[WEATHER_REPLY_SIZE];
    struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 };
    WeatherForecast forecast;

    (void)setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                     sizeof(timeout));
    if (receive_payload(client_fd, payload) == -1)
        snprintf(reply, sizeof(reply), "%s", "ERROR invalid or oversized request\n");
    else if (payload[0] == '{')
    {
        fprintf(stderr, "[Weather] received JSON (%zu bytes): %s\n",
                strlen(payload), payload);
        if (weather_receiver_parse_hourly_json(payload, &forecast) == -1)
        {
            fprintf(stderr, "[Weather] invalid weather JSON\n");
            snprintf(reply, sizeof(reply), "%s", "ERROR invalid weather payload\n");
        }
        else
        {
            fprintf(stderr, "[Weather] parsed city=%s latitude=%.4f longitude=%.4f hours=%d\n",
                    forecast.city, forecast.latitude, forecast.longitude,
                    WEATHER_HOUR_COUNT);
            weather_receiver_store_forecast(&forecast);
            snprintf(reply, sizeof(reply), "%s", "OK weather cached\n");
        }
    }
    else
        handle_text_payload(payload, reply);
    (void)send_reply(client_fd, reply);
    (void)shutdown(client_fd, SHUT_WR);
    close(client_fd);
}

static int receiver_is_running(void)
{
    int running;

    pthread_mutex_lock(&receiver.mutex);
    running = receiver.running;
    pthread_mutex_unlock(&receiver.mutex);
    return running;
}

static void *weather_receiver_thread(void *argument)
{
    (void)argument;

    while (receiver_is_running())
    {
        fd_set read_fds;
        struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
        int listen_fd;
        int ready;

        pthread_mutex_lock(&receiver.mutex);
        listen_fd = receiver.listen_fd;
        pthread_mutex_unlock(&receiver.mutex);
        if (listen_fd < 0)
            break;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        ready = select(listen_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(listen_fd, &read_fds))
        {
            int client_fd = accept(listen_fd, NULL, NULL);

            if (client_fd >= 0)
                handle_client(client_fd);
        }
        else if (ready < 0 && errno != EINTR)
        {
            if (!receiver_is_running())
                break;
            usleep(10 * 1000);
            continue;
        }
        weather_receiver_apply_current_hour();
    }
    return NULL;
}

int weather_receiver_start(void)
{
    struct sockaddr_in address;
    int enabled = 1;
    int listen_fd;
    int thread_error;

    pthread_mutex_lock(&receiver.mutex);
    if (receiver.thread_started)
    {
        pthread_mutex_unlock(&receiver.mutex);
        return 0;
    }
    pthread_mutex_unlock(&receiver.mutex);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        return -1;
    (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(WEATHER_RECEIVER_PORT);
    if (bind(listen_fd, (const struct sockaddr *)&address, sizeof(address)) == -1 ||
        listen(listen_fd, 4) == -1)
    {
        int saved_errno = errno;

        close(listen_fd);
        errno = saved_errno;
        return -1;
    }

    pthread_mutex_lock(&receiver.mutex);
    receiver.listen_fd = listen_fd;
    receiver.running = 1;
    thread_error = pthread_create(&receiver.thread, NULL, weather_receiver_thread,
                                  NULL);
    if (thread_error != 0)
    {
        receiver.running = 0;
        receiver.listen_fd = -1;
        pthread_mutex_unlock(&receiver.mutex);
        close(listen_fd);
        errno = thread_error;
        return -1;
    }
    receiver.thread_started = 1;
    pthread_mutex_unlock(&receiver.mutex);
    return 0;
}

void weather_receiver_stop(void)
{
    pthread_t thread;
    int listen_fd;
    int thread_started;

    pthread_mutex_lock(&receiver.mutex);
    thread_started = receiver.thread_started;
    if (thread_started)
    {
        receiver.running = 0;
        listen_fd = receiver.listen_fd;
        thread = receiver.thread;
    }
    else
        listen_fd = -1;
    pthread_mutex_unlock(&receiver.mutex);
    if (!thread_started)
        return;
    if (listen_fd >= 0)
        (void)shutdown(listen_fd, SHUT_RDWR);
    (void)pthread_join(thread, NULL);
    if (listen_fd >= 0)
        close(listen_fd);
    pthread_mutex_lock(&receiver.mutex);
    if (receiver.listen_fd == listen_fd)
        receiver.listen_fd = -1;
    receiver.thread_started = 0;
    pthread_mutex_unlock(&receiver.mutex);
}

void weather_receiver_set_text_handler(WeatherTextHandler handler, void *user_data)
{
    pthread_mutex_lock(&receiver.mutex);
    receiver.text_handler = handler;
    receiver.text_handler_data = user_data;
    pthread_mutex_unlock(&receiver.mutex);
}
