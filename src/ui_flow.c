//最新改动
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//最新改动

#include "app_config.h"
#include "image_convert.h"
#include "jpeg_codec.h"
//最新改动
#include "overlay.h"
#include "record_indicator.h"
//最新改动
#include "ui_flow.h"


void show_startup_animation(Lcd *lcd)
{
    char *frames[] = {
        "./logo/frame-0.jpeg", "./logo/frame-1.jpeg",
        "./logo/frame-2.jpeg", "./logo/frame-3.jpeg",
        "./logo/frame-4.jpeg", "./logo/main_UI.jpeg"
    };

    for (int round = 0; round < 5; round++)
    {
        for (int i = 0; i < 4; i++)
        {
            read_JPEG_file(frames[i], lcd->pixels);
            usleep(100 * 1000);
        }
    }
    read_JPEG_file(frames[4], lcd->pixels);
    usleep(400 * 1000);
    read_JPEG_file(frames[5], lcd->pixels);
}

//最新改动
typedef struct {
    sem_t lcd_sem;
    pthread_mutex_t state_mutex;
    int live_mode;
    int recording;
    int touch_running;
} UiState;

typedef struct {
    Lcd *lcd;
    Touch *touch;
    Camera *camera;
    Recorder *recorder;
    UiState *state;
} CameraThreadContext;

typedef struct {
    Lcd *lcd;
    UiState *state;
} IndicatorThreadContext;

static int is_camera_entry_touch(int x, int y)
{
    return x > 10 && x < 70 && y > 70 && y < 110;
}

static int is_camera_exit_touch(int x, int y)
{
    return x > 10 && x < 70 && y > 140 && y < 170;
}

static void lcd_sem_wait(UiState *state)
{
    while (sem_wait(&state->lcd_sem) == -1 && errno == EINTR)
    {
    }
}

static int ui_is_live(UiState *state)
{
    int live_mode;

    pthread_mutex_lock(&state->state_mutex);
    live_mode = state->live_mode;
    pthread_mutex_unlock(&state->state_mutex);
    return live_mode;
}

static int ui_is_recording(UiState *state)
{
    int recording;

    pthread_mutex_lock(&state->state_mutex);
    recording = state->recording;
    pthread_mutex_unlock(&state->state_mutex);
    return recording;
}

static void ui_set_live(UiState *state, int live_mode)
{
    pthread_mutex_lock(&state->state_mutex);
    state->live_mode = live_mode;
    if (!live_mode)
        state->recording = 0;
    pthread_mutex_unlock(&state->state_mutex);
}

static void ui_set_recording(UiState *state, int recording)
{
    pthread_mutex_lock(&state->state_mutex);
    state->recording = recording;
    pthread_mutex_unlock(&state->state_mutex);
}

static void show_main_interface(Lcd *lcd, UiState *state)
{
    lcd_sem_wait(state);
    read_JPEG_file("./logo/main_UI.jpeg", lcd->pixels);
    sem_post(&state->lcd_sem);
}

static void *touch_thread_main(void *argument)
{
    CameraThreadContext *context = argument;

    while (context->state->touch_running)
    {
        int x;
        int y;

        if (touch_read(context->touch) == -1)
        {
            usleep(10 * 1000);
            continue;
        }

        touch_get_position(context->touch, &x, &y);
        if (!ui_is_live(context->state) && is_camera_entry_touch(x, y))
            ui_set_live(context->state, 1);
        else if (ui_is_live(context->state) && is_camera_exit_touch(x, y))
            ui_set_live(context->state, 0);
    }
    return NULL;
}

static void *camera_thread_main(void *argument)
{
    CameraThreadContext *context = argument;

    if (camera_open(context->camera, "/dev/video7") == -1 ||
        camera_init(context->camera) == -1 ||
        camera_start(context->camera) == -1)
    {
        camera_close(context->camera);
        ui_set_live(context->state, 0);
        return NULL;
    }

    while (ui_is_live(context->state))
    {
        void *frame_buffer;
        JSAMPLE *image_buffer;
        int touch_x;
        int touch_y;

        if (camera_wait_frame(context->camera) < 0)
            break;
        frame_buffer = camera_dequeue(context->camera);
        if (frame_buffer == NULL)
            break;

        lcd_sem_wait(context->state);
        image_buffer = image_convert_frame(frame_buffer, (unsigned int *)context->lcd->pixels);
        if (image_buffer != NULL)
            parking_assist_lines_drawing(context->lcd->pixels);
        sem_post(&context->state->lcd_sem);
        if (image_buffer == NULL)
            break;

        touch_get_position(context->touch, &touch_x, &touch_y);
        recorder_save_frame(context->recorder, touch_x, image_buffer);
        ui_set_recording(context->state, context->recorder->recording);
        free(image_buffer);

        if (camera_requeue(context->camera) == -1)
            break;
        (void)touch_y;
    }

    camera_close(context->camera);
    ui_set_live(context->state, 0);
    return NULL;
}

static void *red_dot_thread_main(void *argument)
{
    IndicatorThreadContext *context = argument;
    int visible = 0;

    while (ui_is_live(context->state))
    {
        if (ui_is_recording(context->state))
        {
            lcd_sem_wait(context->state);
            record_indicator_draw_dot(context->lcd->pixels, visible);
            sem_post(&context->state->lcd_sem);
            visible = !visible;
            usleep(500 * 1000);
        }
        else
        {
            if (visible)
            {
                lcd_sem_wait(context->state);
                record_indicator_draw_dot(context->lcd->pixels, 0);
                sem_post(&context->state->lcd_sem);
                visible = 0;
            }
            usleep(50 * 1000);
        }
    }

    lcd_sem_wait(context->state);
    record_indicator_draw_dot(context->lcd->pixels, 0);
    sem_post(&context->state->lcd_sem);
    return NULL;
}

static void *rec_thread_main(void *argument)
{
    IndicatorThreadContext *context = argument;
    int shown = 0;

    while (ui_is_live(context->state))
    {
        int recording = ui_is_recording(context->state);

        if (recording && !shown)
        {
            lcd_sem_wait(context->state);
            record_indicator_draw_rec(context->lcd->pixels, 1);
            sem_post(&context->state->lcd_sem);
            shown = 1;
        }
        else if (!recording && shown)
        {
            lcd_sem_wait(context->state);
            record_indicator_draw_rec(context->lcd->pixels, 0);
            sem_post(&context->state->lcd_sem);
            shown = 0;
        }
        usleep(50 * 1000);
    }

    lcd_sem_wait(context->state);
    record_indicator_draw_rec(context->lcd->pixels, 0);
    sem_post(&context->state->lcd_sem);
    return NULL;
}

int ui_run(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder)
{
    UiState state;
    CameraThreadContext camera_context;
    IndicatorThreadContext indicator_context;
    pthread_t touch_thread;

    memset(&state, 0, sizeof(state));
    if (sem_init(&state.lcd_sem, 0, 1) == -1 ||
        pthread_mutex_init(&state.state_mutex, NULL) != 0)
        return -1;

    state.touch_running = 1;
    memset(&camera_context, 0, sizeof(camera_context));
    camera_context.lcd = lcd;
    camera_context.touch = touch;
    camera_context.camera = camera;
    camera_context.recorder = recorder;
    camera_context.state = &state;
    indicator_context.lcd = lcd;
    indicator_context.state = &state;

    if (pthread_create(&touch_thread, NULL, touch_thread_main, &camera_context) != 0)
        return -1;

    for (;;)
    {
        pthread_t camera_thread;
        pthread_t red_dot_thread;
        pthread_t rec_thread;

        while (!ui_is_live(&state))
            usleep(10 * 1000);

        touch_reset_position(touch);
        if (pthread_create(&camera_thread, NULL, camera_thread_main, &camera_context) != 0)
        {
            ui_set_live(&state, 0);
            return -1;
        }
        if (pthread_create(&red_dot_thread, NULL, red_dot_thread_main, &indicator_context) != 0)
        {
            ui_set_live(&state, 0);
            pthread_join(camera_thread, NULL);
            return -1;
        }
        if (pthread_create(&rec_thread, NULL, rec_thread_main, &indicator_context) != 0)
        {
            ui_set_live(&state, 0);
            pthread_join(camera_thread, NULL);
            pthread_join(red_dot_thread, NULL);
            return -1;
        }

        while (ui_is_live(&state))
            usleep(10 * 1000);

        pthread_join(camera_thread, NULL);
        pthread_join(red_dot_thread, NULL);
        pthread_join(rec_thread, NULL);
        recorder_stop_session(recorder);
        show_main_interface(lcd, &state);
        touch_reset_position(touch);
    }
}
//最新改动
