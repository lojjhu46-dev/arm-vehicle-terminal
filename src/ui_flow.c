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
// 判断触摸坐标是否命中主界面的实时影像入口。
static int is_camera_entry_touch(int x, int y)
{
    return x > 10 && x < 70 && y > 70 && y < 110;
}
//最新改动

#define UI_ACTION_RECORD_TOGGLE 0x01U
#define UI_ACTION_PHOTO 0x02U

//最新改动
// 获取 LCD 二值信号量，并在信号中断时继续等待。
static void lcd_sem_wait(UiState *state)
{
    while (sem_wait(&state->lcd_sem) == -1 && errno == EINTR)
    {
    }
}
//最新改动

//最新改动
// 在线程安全条件下读取是否处于实时影像模式。
static int ui_is_live(UiState *state)
{
    int live_mode;
    //更新录像状态
    pthread_mutex_lock(&state->state_mutex);
    live_mode = state->live_mode;
    pthread_mutex_unlock(&state->state_mutex);
    return live_mode;
}
//最新改动

//最新改动
// 在线程安全条件下读取当前录像状态。
static int ui_is_recording(UiState *state)
{
    int recording;

    pthread_mutex_lock(&state->state_mutex);
    recording = state->recording;
    pthread_mutex_unlock(&state->state_mutex);
    return recording;
}
//最新改动

//最新改动
// 更新实时影像状态；退出实时影像时同时清除录像状态。
static void ui_set_live(UiState *state, int live_mode)
{
    pthread_mutex_lock(&state->state_mutex);
    state->live_mode = live_mode;
    if (!live_mode)
        state->recording = 0;
    pthread_mutex_unlock(&state->state_mutex);
}
//最新改动

//最新改动
// 更新录像状态，供红点和 REC 线程查询。
static void ui_set_recording(UiState *state, int recording)
{
    pthread_mutex_lock(&state->state_mutex);
    state->recording = recording;
    pthread_mutex_unlock(&state->state_mutex);
}
//最新改动

static int ui_toggle_parking_guides(UiState *state)
{
    int enabled;

    pthread_mutex_lock(&state->state_mutex);
    state->parking_guides_enabled = !state->parking_guides_enabled;
    enabled = state->parking_guides_enabled;
    pthread_mutex_unlock(&state->state_mutex);
    return enabled;
}

static int ui_parking_guides_enabled(UiState *state)
{
    int enabled;

    pthread_mutex_lock(&state->state_mutex);
    enabled = state->parking_guides_enabled;
    pthread_mutex_unlock(&state->state_mutex);
    return enabled;
}

static void ui_queue_action(UiState *state, unsigned int action)
{
    pthread_mutex_lock(&state->state_mutex);
    state->pending_actions |= action;
    pthread_mutex_unlock(&state->state_mutex);
}

static unsigned int ui_take_actions(UiState *state)
{
    unsigned int actions;

    pthread_mutex_lock(&state->state_mutex);
    actions = state->pending_actions;
    state->pending_actions = 0;
    pthread_mutex_unlock(&state->state_mutex);
    return actions;
}

//最新改动
// 在获得 LCD 信号量后刷新主界面图片。
static void show_main_interface(Lcd *lcd, UiState *state)
{
    lcd_sem_wait(state);
    read_JPEG_file("./logo/main_UI.jpeg", lcd->pixels);
    sem_post(&state->lcd_sem);
}
//最新改动

//最新改动
// 持续读取完整点击，并将其路由到主界面或实时画面的对应按钮。
static void *touch_thread_main(void *argument)
{
    CameraThreadContext *context = argument;

    while (context->state->touch_running)
    {
        int x;
        int y;

        int read_result = touch_read_tap(context->touch, &x, &y);

        if (read_result == -1)
        {
            usleep(10 * 1000);
            continue;
        }
        if (read_result == 0)
            continue;

        if (!ui_is_live(context->state) && is_camera_entry_touch(x, y))
            ui_set_live(context->state, 1);
        else if (ui_is_live(context->state))
        {
            switch (live_controls_hit_test(x, y))
            {
            case LIVE_CONTROL_EXIT:
                ui_set_live(context->state, 0);
                break;
            case LIVE_CONTROL_RECORD:
                ui_queue_action(context->state, UI_ACTION_RECORD_TOGGLE);
                break;
            case LIVE_CONTROL_GUIDES:
                (void)ui_toggle_parking_guides(context->state);
                break;
            case LIVE_CONTROL_PHOTO:
                ui_queue_action(context->state, UI_ACTION_PHOTO);
                break;
            case LIVE_CONTROL_NONE:
                break;
            }
        }
    }
    return NULL;
}
//最新改动

//最新改动
// 独占摄像头设备，完成取帧、显示、辅助线和录像保存。
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
        unsigned int actions;
        int guides_enabled;

        if (camera_wait_frame(context->camera) < 0)
            break;
        frame_buffer = camera_dequeue(context->camera);
        if (frame_buffer == NULL)
            break;

        actions = ui_take_actions(context->state);
        if (actions & UI_ACTION_RECORD_TOGGLE)
        {
            if (context->recorder->recording)
                recorder_stop_session(context->recorder);
            else
                (void)recorder_start_session(context->recorder, RECORD_ROOT_DIR);
        }
        ui_set_recording(context->state, context->recorder->recording);
        guides_enabled = ui_parking_guides_enabled(context->state);

        lcd_sem_wait(context->state);
        image_buffer = image_convert_frame(frame_buffer, (unsigned int *)context->lcd->pixels);
        if (image_buffer != NULL && guides_enabled)
            parking_assist_lines_drawing(context->lcd->pixels);
        if (image_buffer != NULL)
            live_controls_draw(context->controls, context->lcd->pixels,
                               context->recorder->recording, guides_enabled);
        sem_post(&context->state->lcd_sem);
        if (image_buffer == NULL)
            break;

        if (actions & UI_ACTION_PHOTO)
            (void)recorder_save_photo(context->recorder, PHOTO_DIR, image_buffer);
        (void)recorder_save_frame(context->recorder, image_buffer);
        ui_set_recording(context->state, context->recorder->recording);
        free(image_buffer);

        if (camera_requeue(context->camera) == -1)
            break;
    }

    camera_close(context->camera);
    ui_set_live(context->state, 0);
    return NULL;
}
//最新改动

//最新改动
// 每 0.5 秒切换一次录像红点的显示状态。
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
//最新改动

//最新改动
// 在录像期间固定显示 REC，停止录像或退出时清除它。
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
//最新改动

//最新改动
// 初始化同步对象并协调触摸、摄像头、红点和 REC 线程的生命周期。
int ui_run(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder)
{
    UiState state;
    CameraThreadContext camera_context;
    IndicatorThreadContext indicator_context;
    LiveControls controls;
    pthread_t touch_thread;

    memset(&state, 0, sizeof(state));
    if (sem_init(&state.lcd_sem, 0, 1) == -1 ||
        pthread_mutex_init(&state.state_mutex, NULL) != 0)
        return -1;

    state.touch_running = 1;
    state.parking_guides_enabled = 1;
    if (live_controls_init(&controls) == -1)
    {
        fprintf(stderr, "missing live-control assets in %s; deploy the resource directory\n",
                RESOURCE_DIR);
        pthread_mutex_destroy(&state.state_mutex);
        sem_destroy(&state.lcd_sem);
        return -1;
    }
    memset(&camera_context, 0, sizeof(camera_context));
    camera_context.lcd = lcd;
    camera_context.touch = touch;
    camera_context.camera = camera;
    camera_context.recorder = recorder;
    camera_context.controls = &controls;
    camera_context.state = &state;
    indicator_context.lcd = lcd;
    indicator_context.state = &state;

    if (pthread_create(&touch_thread, NULL, touch_thread_main, &camera_context) != 0)
    {
        live_controls_destroy(&controls);
        pthread_mutex_destroy(&state.state_mutex);
        sem_destroy(&state.lcd_sem);
        return -1;
    }

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
