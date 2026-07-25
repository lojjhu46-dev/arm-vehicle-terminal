#include <sys/time.h>

#include "ui_flow.h"

//最新修改
// 将静态 JPEG 同步写入所有 framebuffer 页面。
static void show_image_all_pages(Lcd *lcd, const char *filename)
{
    for (int page = 0; page < lcd_page_count(lcd); ++page)
        read_JPEG_file(filename, lcd_page_pixels(lcd, page));
    (void)lcd_show_page(lcd, 0);
}
//最新修改

void show_startup_animation(Lcd *lcd)
{
    char *frames[] = {
        "./resource/logo/frame-0.jpeg", "./resource/logo/frame-1.jpeg",
        "./resource/logo/frame-2.jpeg", "./resource/logo/frame-3.jpeg",
        "./resource/logo/frame-4.jpeg", "./resource/logo/mian_UI.jpeg"
    };

    for (int round = 0; round < 5; round++)
    {
        for (int i = 0; i < 4; i++)
        {
            show_image_all_pages(lcd, frames[i]);
            usleep(100 * 1000);
        }
    }
    show_image_all_pages(lcd, frames[4]);
    usleep(400 * 1000);
    show_image_all_pages(lcd, frames[5]);
}

//最新改动
// 判断触摸坐标是否命中主界面的实时影像入口。
static int is_camera_entry_touch(int x, int y)
{
    return x > 45 && x < 160 && y > 160 && y < 305;
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

//最新修改
// 线程安全地读取回放状态。
static int ui_is_playback(UiState *state)
{
    int playback_mode;

    pthread_mutex_lock(&state->state_mutex);
    playback_mode = state->playback_mode;
    pthread_mutex_unlock(&state->state_mutex);
    return playback_mode;
}

// 从实时画面切换到历史录像回放。
static void ui_start_playback(UiState *state)
{
    pthread_mutex_lock(&state->state_mutex);
    state->playback_mode = 1;
    state->live_mode = 0;
    state->recording = 0;
    pthread_mutex_unlock(&state->state_mutex);
}

// 退出历史录像回放。
static void ui_stop_playback(UiState *state)
{
    pthread_mutex_lock(&state->state_mutex);
    state->playback_mode = 0;
    pthread_mutex_unlock(&state->state_mutex);
}
//最新修改

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
    show_image_all_pages(lcd, "./resource/logo/mian_UI.jpeg");
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

        if (ui_is_playback(context->state))
        {
            if (live_controls_hit_test(x, y) == LIVE_CONTROL_EXIT)
                ui_stop_playback(context->state);
        }
        else if (!ui_is_live(context->state) && is_camera_entry_touch(x, y))
            ui_set_live(context->state, 1);
        else if (ui_is_live(context->state))
        {
            switch (live_controls_hit_test(x, y))
            {
            case LIVE_CONTROL_EXIT:
                ui_set_live(context->state, 0);
                break;
            case LIVE_CONTROL_FILE_MANAGER:
                break;
            case LIVE_CONTROL_RECORD:
                ui_queue_action(context->state, UI_ACTION_RECORD_TOGGLE);
                break;
            case LIVE_CONTROL_VIEW_RECORDINGS:
                ui_start_playback(context->state);
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

//最新修改
// 将单张录像 JPEG 居中绘制到摄像头显示区域。
static int draw_playback_frame(int *framebuffer, const char *filename)
{
    int *pixels;
    int width;
    int height;
    int copy_width;
    int copy_height;
    int source_x;
    int source_y;
    int target_x;
    int target_y;

    if (read_JPEG_image(filename, &pixels, &width, &height) == -1)
        return -1;
    for (int y = 0; y < LCD_HEIGHT; ++y)
    {
        for (int x = CAMERA_DISPLAY_X; x < CAMERA_DISPLAY_X + CAMERA_FRAME_W; ++x)
            framebuffer[y * LCD_WIDTH + x] = 0x000000;
    }
    copy_width = width > CAMERA_FRAME_W ? CAMERA_FRAME_W : width;
    copy_height = height > CAMERA_FRAME_H ? CAMERA_FRAME_H : height;
    source_x = (width - copy_width) / 2;
    source_y = (height - copy_height) / 2;
    target_x = CAMERA_DISPLAY_X + (CAMERA_FRAME_W - copy_width) / 2;
    target_y = (CAMERA_FRAME_H - copy_height) / 2;
    for (int y = 0; y < copy_height; ++y)
    {
        for (int x = 0; x < copy_width; ++x)
            framebuffer[(target_y + y) * LCD_WIDTH + target_x + x] =
                pixels[(source_y + y) * width + source_x + x];
    }
    free(pixels);
    return 0;
}

// 按约 33 帧每秒播放最新录像会话。
static void playback_run(Lcd *lcd, UiState *state, LiveControls *controls)
{
    PlaybackFrames frames;
    unsigned int page_initialized[LCD_MAX_PAGES] = {0};
    size_t frame_index = 0;

    if (playback_load_latest(&frames, RECORD_ROOT_DIR) == -1)
    {
        ui_stop_playback(state);
        return;
    }
    while (ui_is_playback(state))
    {
        int page;

        lcd_sem_wait(state);
        if (lcd_begin_frame(lcd) == -1)
        {
            sem_post(&state->lcd_sem);
            break;
        }
        page = lcd_draw_page(lcd);
        if (!page_initialized[page])
        {
            live_controls_draw_playback(controls, lcd->pixels);
            page_initialized[page] = 1;
        }
        if (draw_playback_frame(lcd->pixels, frames.paths[frame_index]) == 0)
            (void)lcd_present(lcd);
        sem_post(&state->lcd_sem);

        frame_index = (frame_index + 1) % frames.count;
        usleep(1000000 / 33);
    }
    ui_stop_playback(state);
}
//最新修改

//最新修改
// 摄像头线程独占后台页，整帧完成后再翻转显示。
static void *camera_thread_main(void *argument)
{
    CameraThreadContext *context = argument;
    ImageConverter converter;                 // 复用的 RGB 转换缓冲。
    unsigned int page_versions[LCD_MAX_PAGES] = {0}; // 每页侧栏版本。
    unsigned int sidebar_version = 1;         // 当前侧栏状态版本。
    int last_guides_enabled = -1;             // 最近侧栏辅助线状态。
    int last_recording = -1;                  // 最近侧栏录像状态。

    if (camera_open(context->camera, "/dev/video7") == -1 ||
        camera_init(context->camera) == -1 ||
        camera_start(context->camera) == -1 ||
        image_converter_init(&converter) == -1)
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
        struct timeval now;
        int guides_enabled;
        int recording;
        int dot_visible;
        int page;

        if (camera_wait_frame(context->camera) < 0)
            break;
        frame_buffer = camera_dequeue(context->camera);
        if (frame_buffer == NULL)
            break;

        actions = ui_take_actions(context->state);
        if (actions & UI_ACTION_RECORD_TOGGLE)
        {
            if (recorder_is_recording(context->recorder))
                recorder_stop_session(context->recorder);
            else
                (void)recorder_start_session(context->recorder, RECORD_ROOT_DIR);
        }
        recording = recorder_is_recording(context->recorder);
        ui_set_recording(context->state, recording);
        guides_enabled = ui_parking_guides_enabled(context->state);
        if (guides_enabled != last_guides_enabled || recording != last_recording)
        {
            ++sidebar_version;
            last_guides_enabled = guides_enabled;
            last_recording = recording;
        }

        lcd_sem_wait(context->state);
        if (lcd_begin_frame(context->lcd) == -1)
        {
            sem_post(&context->state->lcd_sem);
            break;
        }
        page = lcd_draw_page(context->lcd);
        image_buffer = image_convert_frame(&converter, frame_buffer,
                                           (unsigned int *)context->lcd->pixels);
        if (image_buffer != NULL && guides_enabled)
            parking_assist_lines_drawing(context->lcd->pixels);
        if (image_buffer != NULL && page_versions[page] != sidebar_version)
        {
            live_controls_draw(context->controls, context->lcd->pixels, recording,
                               guides_enabled, 1);
            page_versions[page] = sidebar_version;
        }
        gettimeofday(&now, NULL);
        dot_visible = recording && ((now.tv_sec * 2 + now.tv_usec / 500000) & 1);
        record_indicator_draw_dot(context->lcd->pixels, dot_visible);
        record_indicator_draw_rec(context->indicator, context->lcd->pixels, recording);
        if (lcd_present(context->lcd) == -1)
        {
            sem_post(&context->state->lcd_sem);
            break;
        }
        sem_post(&context->state->lcd_sem);
        if (image_buffer == NULL)
            break;

        if (actions & UI_ACTION_PHOTO)
            (void)recorder_save_photo(context->recorder, PHOTO_DIR, image_buffer);
        (void)recorder_submit_frame(context->recorder, image_buffer);
        ui_set_recording(context->state, recorder_is_recording(context->recorder));

        if (camera_requeue(context->camera) == -1)
            break;
    }

    image_converter_destroy(&converter);
    camera_close(context->camera);
    ui_set_live(context->state, 0);
    return NULL;
}
//最新修改

//最新修改
// 初始化同步对象并协调触摸与三页摄像头线程生命周期。
int ui_run(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder)
{
    UiState state;
    CameraThreadContext camera_context;
    LiveControls controls;
    RecordIndicator indicator;
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
    if (record_indicator_init(&indicator) == -1)
    {
        fprintf(stderr, "missing REC asset in %s\n", RESOURCE_DIR);
        live_controls_destroy(&controls);
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
    camera_context.indicator = &indicator;
    camera_context.state = &state;

    if (pthread_create(&touch_thread, NULL, touch_thread_main, &camera_context) != 0)
    {
        record_indicator_destroy(&indicator);
        live_controls_destroy(&controls);
        pthread_mutex_destroy(&state.state_mutex);
        sem_destroy(&state.lcd_sem);
        return -1;
    }

    for (;;)
    {
        pthread_t camera_thread;

        while (!ui_is_live(&state) && !ui_is_playback(&state))
            usleep(10 * 1000);

        if (ui_is_playback(&state))
        {
            touch_reset_position(touch);
            playback_run(lcd, &state, &controls);
            show_main_interface(lcd, &state);
            continue;
        }

        touch_reset_position(touch);
        if (pthread_create(&camera_thread, NULL, camera_thread_main, &camera_context) != 0)
        {
            ui_set_live(&state, 0);
            return -1;
        }

        while (ui_is_live(&state))
            usleep(10 * 1000);

        pthread_join(camera_thread, NULL);
        recorder_stop_session(recorder);
        if (ui_is_playback(&state))
        {
            recorder_wait_idle(recorder);
            touch_reset_position(touch);
            playback_run(lcd, &state, &controls);
        }
        show_main_interface(lcd, &state);
        touch_reset_position(touch);
    }
}
//最新修改
