#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "main_weather.h"
#include "ui_flow.h"
#include "freetype_lcd.h"

//最新修改
// 将静态 JPEG 同步写入所有 framebuffer 页面。
static void show_image_all_pages(Lcd *lcd, const char *filename)
{
    for (int page = 0; page < lcd_page_count(lcd); ++page)
        read_JPEG_file(filename, lcd_page_pixels(lcd, page));
    (void)lcd_show_page(lcd, 0);
}
//最新修改

static time_t main_dashboard_minute = (time_t)-1;
static unsigned long main_dashboard_weather_version;

// 返回当前本地时间对应的分钟序号。
static time_t current_minute(void)
{
    struct timeval now;

    if (gettimeofday(&now, NULL) == -1)
        return (time_t)-1;
    return (time_t)(now.tv_sec / 60);
}

// 判断时钟或天气版本是否要求重绘主界面。
static int main_dashboard_refresh_due(void)
{
    return main_dashboard_minute != current_minute() ||
           main_dashboard_weather_version != main_weather_version();
}

// 在主界面底图上叠加时间、日期和天气信息。
static void draw_main_dashboard(Lcd *lcd)
{
    static const char *const weekdays[] = {
        "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"
    };
    struct timeval now;
    struct tm local_time;
    MainWeather weather;
    unsigned long weather_version;
    char clock_text[8];
    char date_text[32];
    char date_part[16];
    char temperature_text[16];
    char weather_text[96];

    gettimeofday(&now, NULL);
    if (localtime_r(&now.tv_sec, &local_time) == NULL)
        return;
    if (strftime(clock_text, sizeof(clock_text), "%H:%M", &local_time) == 0 ||
        strftime(date_part, sizeof(date_part), "%Y/%m/%d", &local_time) == 0)
        return;
    snprintf(date_text, sizeof(date_text), "%s  %s", date_part,
             weekdays[local_time.tm_wday]);
    if (main_weather_get(&weather, &weather_version) == -1)
        return;
    fprintf(stderr, "[UI] refreshing main dashboard weather_version=%lu city=%s temp=%d type=%s\n",
            weather_version, weather.city, weather.temperature_c,
            main_weather_label(weather.type));
    snprintf(temperature_text, sizeof(temperature_text), "%d℃",
             weather.temperature_c);
    snprintf(weather_text, sizeof(weather_text), "%s | %s", weather.city,
             main_weather_label(weather.type));

    (void)main_weather_draw_icon(lcd, &weather);

    for (int page = 0; page < lcd_page_count(lcd); ++page)
    {
        freetype_set_lcd((unsigned int *)lcd_page_pixels(lcd, page));

        freetype_set_size(52);
        lcd_draw_text(120, 105, clock_text, 0xf4f4f4);
        freetype_set_size(18);
        lcd_draw_text(123, 129, date_text, 0x9c9c9c);
        freetype_set_size(54);
        lcd_draw_text(558, 105, temperature_text, 0xf4f4f4);
        freetype_set_size(18);
        lcd_draw_text(565, 129, weather_text, 0x9c9c9c);
    }
    main_dashboard_minute = (time_t)(now.tv_sec / 60);
    main_dashboard_weather_version = weather_version;
    (void)lcd_show_page(lcd, 0);
}

// 依次刷写开机动画帧，结束后显示主界面。
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
    draw_main_dashboard(lcd);
}

//最新改动
// 判断触摸坐标是否命中主界面的实时影像入口。
static int is_camera_entry_touch(int x, int y)
{
    return x > 45 && x < 160 && y > 160 && y < 305;
}

// 判断触摸坐标是否命中主界面的文件管理入口。
static int is_main_file_manager_touch(int x, int y)
{
    return x > 195 && x < 310 && y > 160 && y < 305;
}

// 主界面右下角的“更多应用”入口。
static int is_more_apps_touch(int x, int y)
{
    return x > 635 && x < 770 && y > 325 && y < 465;
}

// 扩展页底部中央的“去登录”按钮。
static int is_expand_login_touch(int x, int y)
{
    return x > 345 && x < 585 && y > 375 && y < 435;
}

//最新改动

#define UI_ACTION_RECORD_TOGGLE 0x01U
#define UI_ACTION_PHOTO 0x02U
#define UI_ACTION_RECORD_START 0x04U
#define UI_ACTION_RECORD_STOP 0x08U

#define BACK_BUTTON_X 5
#define BACK_BUTTON_Y 5
#define BACK_BUTTON_WIDTH 60
#define BACK_BUTTON_HEIGHT 68

typedef struct {
    int *pixels;
    int width;
    int height;
} BackButton;

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

// 在线程安全条件下读取触摸线程运行标志。
static int ui_touch_running(UiState *state)
{
    int running;

    pthread_mutex_lock(&state->state_mutex);
    running = state->touch_running;
    pthread_mutex_unlock(&state->state_mutex);
    return running;
}

// 在线程安全条件下读取当前图片页面状态。
static int ui_get_screen(UiState *state)
{
    int screen;

    pthread_mutex_lock(&state->state_mutex);
    screen = state->screen;
    pthread_mutex_unlock(&state->state_mutex);
    return screen;
}

// 切换图片页面状态，由主循环负责实际刷图。
static void ui_set_screen(UiState *state, int screen)
{
    pthread_mutex_lock(&state->state_mutex);
    state->screen = screen;
    pthread_mutex_unlock(&state->state_mutex);
}

// 返回会员页面链路中的上一级页面。
static int ui_previous_screen(int screen)
{
    switch (screen)
    {
    case UI_SCREEN_SANCTUARY:
        return UI_SCREEN_LOGIN;
    case UI_SCREEN_LOGIN:
        return UI_SCREEN_EXPAND;
    case UI_SCREEN_EXPAND:
    default:
        return UI_SCREEN_MAIN;
    }
}

// 判断触摸点是否命中图片页面左上角返回按钮。
static int is_back_button_touch(int x, int y)
{
    return x >= BACK_BUTTON_X && x < BACK_BUTTON_X + BACK_BUTTON_WIDTH &&
           y >= BACK_BUTTON_Y && y < BACK_BUTTON_Y + BACK_BUTTON_HEIGHT;
}

// 处理登录页点击，并将短信发送或登录成功动作交给对应模块。
static void ui_handle_member_login_tap(CameraThreadContext *context, int x, int y)
{
    MemberSmsRequest request;
    MemberLoginAction action;

    action = member_login_handle_tap(context->member_login, x, y, &request);
    if (action == MEMBER_LOGIN_ACTION_SEND_SMS)
    {
        if (member_sms_worker_start(context->sms_worker, context->member_login,
                                    &request) == -1)
            member_login_set_sms_start_failed(context->member_login, &request);
    }
    else if (action == MEMBER_LOGIN_ACTION_SUCCESS)
    {
        member_login_reset(context->member_login);
        ui_set_screen(context->state, UI_SCREEN_SANCTUARY);
    }
}

// 读取是否有待执行的文件浏览器请求。
static int ui_is_file_browser_requested(UiState *state)
{
    int requested;

    pthread_mutex_lock(&state->state_mutex);
    requested = state->file_browser_requested;
    pthread_mutex_unlock(&state->state_mutex);
    return requested;
}

// 请求切换到文件浏览器，并停止触摸线程让 LVGL 独占输入。
static void ui_request_file_browser(UiState *state)
{
    pthread_mutex_lock(&state->state_mutex);
    state->file_browser_requested = 1;
    state->live_mode = 0;
    state->recording = 0;
    state->touch_running = 0;
    pthread_mutex_unlock(&state->state_mutex);
}

// 文件浏览器结束后恢复触摸线程及可选回放状态。
static void ui_finish_file_browser(UiState *state, int playback)
{
    pthread_mutex_lock(&state->state_mutex);
    state->file_browser_requested = 0;
    state->touch_running = 0;
    state->playback_mode = playback;
    state->live_mode = 0;
    pthread_mutex_unlock(&state->state_mutex);
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

// 切换倒车辅助线并返回切换后的状态。
static int ui_toggle_parking_guides(UiState *state)
{
    int enabled;

    pthread_mutex_lock(&state->state_mutex);
    state->parking_guides_enabled = !state->parking_guides_enabled;
    enabled = state->parking_guides_enabled;
    pthread_mutex_unlock(&state->state_mutex);
    return enabled;
}

// 在线程安全条件下读取倒车辅助线开关。
static int ui_parking_guides_enabled(UiState *state)
{
    int enabled;

    pthread_mutex_lock(&state->state_mutex);
    enabled = state->parking_guides_enabled;
    pthread_mutex_unlock(&state->state_mutex);
    return enabled;
}

// 按指定值更新倒车辅助线开关。
static void ui_set_parking_guides(UiState *state, int enabled)
{
    pthread_mutex_lock(&state->state_mutex);
    state->parking_guides_enabled = enabled;
    pthread_mutex_unlock(&state->state_mutex);
}

// 将摄像头动作合并到待执行位图。
static void ui_queue_action(UiState *state, unsigned int action)
{
    pthread_mutex_lock(&state->state_mutex);
    state->pending_actions |= action;
    pthread_mutex_unlock(&state->state_mutex);
}

// 取走摄像头线程待执行动作，确保每条动作只消费一次。
static unsigned int ui_take_actions(UiState *state)
{
    unsigned int actions;

    pthread_mutex_lock(&state->state_mutex);
    actions = state->pending_actions;
    state->pending_actions = 0;
    pthread_mutex_unlock(&state->state_mutex);
    return actions;
}

// 将远程命令邮箱转换为 UI 状态和摄像头待执行动作。
static void ui_apply_remote_commands(UiState *state, UiRemoteCommands *commands)
{
    UiRemoteCommandRequest request;

    ui_remote_commands_take(commands, &request);
    if (request.parking_guides_target >= 0)
        ui_set_parking_guides(state, request.parking_guides_target);
    if (request.recording_target == 1)
    {
        fprintf(stderr, "[UI] remote start-recording request queued\n");
        ui_queue_action(state, UI_ACTION_RECORD_START);
        ui_set_live(state, 1);
    }
    else if (request.recording_target == 0 && ui_is_live(state))
    {
        fprintf(stderr, "[UI] remote stop-recording request queued\n");
        ui_queue_action(state, UI_ACTION_RECORD_STOP);
    }
    if (request.take_photo)
    {
        fprintf(stderr, "[UI] remote photo request queued\n");
        ui_queue_action(state, UI_ACTION_PHOTO);
        ui_set_live(state, 1);
    }
}

// 在 LCD 写入锁保护下刷主界面底图和动态信息。
static void show_main_interface(Lcd *lcd, UiState *state)
{
    lcd_sem_wait(state);
    show_image_all_pages(lcd, "./resource/logo/mian_UI.jpeg");
    draw_main_dashboard(lcd);
    sem_post(&state->lcd_sem);
}
//最新改动

// 持续读取完整点击，并按当前页面或实时画面分发操作。
static void *touch_thread_main(void *argument)
{
    CameraThreadContext *context = argument;

    while (ui_touch_running(context->state))
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
        else if (!ui_is_live(context->state) &&
                 ui_get_screen(context->state) != UI_SCREEN_MAIN &&
                 is_back_button_touch(x, y))
        {
            int screen = ui_get_screen(context->state);

            if (screen == UI_SCREEN_LOGIN)
                member_login_reset(context->member_login);
            ui_set_screen(context->state, ui_previous_screen(screen));
        }
        else if (!ui_is_live(context->state) &&
                 ui_get_screen(context->state) == UI_SCREEN_MAIN &&
                 is_more_apps_touch(x, y))
            ui_set_screen(context->state, UI_SCREEN_EXPAND);
        else if (!ui_is_live(context->state) &&
                 ui_get_screen(context->state) == UI_SCREEN_EXPAND &&
                 is_expand_login_touch(x, y))
        {
            member_login_reset(context->member_login);
            ui_set_screen(context->state, UI_SCREEN_LOGIN);
        }
        else if (!ui_is_live(context->state) &&
                 ui_get_screen(context->state) == UI_SCREEN_LOGIN)
            ui_handle_member_login_tap(context, x, y);
        else if (!ui_is_live(context->state) &&
                 ui_get_screen(context->state) == UI_SCREEN_MAIN &&
                 is_camera_entry_touch(x, y))
            ui_set_live(context->state, 1);
        else if (!ui_is_live(context->state) &&
                 ui_get_screen(context->state) == UI_SCREEN_MAIN &&
                 is_main_file_manager_touch(x, y))
            ui_request_file_browser(context->state);
        else if (ui_is_live(context->state))
        {
            switch (live_controls_hit_test(x, y))
            {
            case LIVE_CONTROL_EXIT:
                ui_set_live(context->state, 0);
                break;
            case LIVE_CONTROL_FILE_MANAGER:
                ui_request_file_browser(context->state);
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
static void playback_run(Lcd *lcd, UiState *state, LiveControls *controls,
                         const char *path, int load_latest,
                         UiRemoteCommands *remote_commands)
{
    PlaybackFrames frames;
    unsigned int page_initialized[LCD_MAX_PAGES] = {0};
    size_t frame_index = 0;

    if ((load_latest ? playback_load_latest(&frames, path) :
         playback_load_session(&frames, path)) == -1)
    {
        ui_stop_playback(state);
        return;
    }
    while (ui_is_playback(state))
    {
        int page;

        ui_apply_remote_commands(state, remote_commands);
        if (!ui_is_playback(state))
            break;
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
        fprintf(stderr, "[UI] camera thread initialization failed\n");
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
        if (actions & UI_ACTION_RECORD_STOP)
        {
            fprintf(stderr, "[UI] executing stop-recording action\n");
            recorder_stop_session(context->recorder);
        }
        else if (actions & UI_ACTION_RECORD_START)
        {
            if (!recorder_is_recording(context->recorder))
            {
                if (recorder_start_session(context->recorder, RECORDINGS_DIR) == -1)
                    fprintf(stderr, "[UI] failed to start recording session under %s\n",
                            RECORDINGS_DIR);
                else
                    fprintf(stderr, "[UI] recording session started: %s\n",
                            context->recorder->dirbuffer);
            }
        }
        else if (actions & UI_ACTION_RECORD_TOGGLE)
        {
            if (recorder_is_recording(context->recorder))
            {
                fprintf(stderr, "[UI] executing stop-recording toggle\n");
                recorder_stop_session(context->recorder);
            }
            else
            {
                if (recorder_start_session(context->recorder, RECORDINGS_DIR) == -1)
                    fprintf(stderr, "[UI] failed to start recording toggle under %s\n",
                            RECORDINGS_DIR);
                else
                    fprintf(stderr, "[UI] recording session started: %s\n",
                            context->recorder->dirbuffer);
            }
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
// 查询远程实时请求，用于中断文件浏览器循环。
static int ui_file_browser_interrupt(void *argument)
{
    return ui_remote_commands_has_live_request(argument);
}

// 暂停普通触摸流程，运行 LVGL 浏览器并恢复后续页面状态。
static int ui_run_file_browser(Lcd *lcd, Touch *touch, Recorder *recorder,
                               UiState *state, LiveControls *controls,
                               CameraThreadContext *camera_context,
                               pthread_t *touch_thread, int *touch_thread_running,
                               UiRemoteCommands *remote_commands,
                               LvglRuntime *runtime, LvglUi *lvgl_ui)
{
    LvglBrowserSelection selection;

    recorder_stop_session(recorder);
    recorder_wait_idle(recorder);
    if (*touch_thread_running)
    {
        pthread_join(*touch_thread, NULL);
        *touch_thread_running = 0;
    }
    touch_reset_position(touch);
    if (lvgl_file_browser_run(runtime, RECORD_ROOT_DIR, &selection,
                              ui_file_browser_interrupt, remote_commands) == -1)
        memset(&selection, 0, sizeof(selection));
    ui_finish_file_browser(state, selection.result == LVGL_BROWSER_PLAYBACK);
    ui_apply_remote_commands(state, remote_commands);
    if (selection.result == LVGL_BROWSER_PLAYBACK && !ui_is_live(state))
    {
        pthread_mutex_lock(&state->state_mutex);
        state->touch_running = 1;
        pthread_mutex_unlock(&state->state_mutex);
        if (pthread_create(touch_thread, NULL, touch_thread_main, camera_context) != 0)
            return -1;
        *touch_thread_running = 1;
        touch_reset_position(touch);
        playback_run(lcd, state, controls, selection.path, 0, remote_commands);
        pthread_mutex_lock(&state->state_mutex);
        state->touch_running = 0;
        pthread_mutex_unlock(&state->state_mutex);
        pthread_join(*touch_thread, NULL);
        *touch_thread_running = 0;
    }
    if (!ui_is_live(state))
        lvgl_ui_show_main(lvgl_ui);
    touch_reset_position(touch);
    return 0;
}

// 返回指定图片页面对应的 JPEG 资源路径。
static const char *ui_screen_image(int screen)
{
    switch (screen)
    {
    case UI_SCREEN_EXPAND:
        return RESOURCE_DIR "/expand.jpg";
    case UI_SCREEN_LOGIN:
        return RESOURCE_DIR "/Menberslogin.jpg";
    case UI_SCREEN_SANCTUARY:
        return RESOURCE_DIR "/MembersSanctuary.jpg";
    default:
        return RESOURCE_DIR "/logo/mian_UI.jpeg";
    }
}

// 加载并校验所有会员图片页面共用的返回按钮资源。
static int back_button_init(BackButton *button)
{
    memset(button, 0, sizeof(*button));
    if (read_JPEG_image(RESOURCE_DIR "/back.jpg", &button->pixels,
                        &button->width, &button->height) == -1)
        return -1;
    if (button->width != BACK_BUTTON_WIDTH || button->height != BACK_BUTTON_HEIGHT)
    {
        free(button->pixels);
        memset(button, 0, sizeof(*button));
        return -1;
    }
    return 0;
}

// 释放返回按钮像素缓冲。
static void back_button_destroy(BackButton *button)
{
    free(button->pixels);
    memset(button, 0, sizeof(*button));
}

// 将返回按钮绘制到图片页面左上角。
static void draw_back_button(int *framebuffer, const BackButton *button)
{
    for (int y = 0; y < button->height; ++y)
    {
        for (int x = 0; x < button->width; ++x)
            framebuffer[(BACK_BUTTON_Y + y) * LCD_WIDTH + BACK_BUTTON_X + x] =
                button->pixels[y * button->width + x];
    }
}

// 用 FreeType 将登录状态快照叠加到登录图片的输入栏。
static void draw_member_login_fields(int *framebuffer,
                                     const MemberLoginSnapshot *snapshot)
{
    freetype_set_lcd((unsigned int *)framebuffer);
    freetype_set_size(22);
    if (snapshot->phone[0] != '\0')
        lcd_draw_text(420, 134, snapshot->phone, 0xe8edf7);
    if (snapshot->code[0] != '\0')
        lcd_draw_text(420, 188, snapshot->code, 0xe8edf7);
    if (snapshot->status[0] != '\0')
    {
        freetype_set_size(15);
        lcd_draw_text(384, 278, snapshot->status, 0xffc36d);
    }
}

// 刷写会员图片页面；登录页额外叠加返回按钮和输入状态。
static void show_screen_image(Lcd *lcd, UiState *state, int screen,
                              const BackButton *back_button,
                              MemberLogin *member_login,
                              unsigned long *login_version)
{
    MemberLoginSnapshot login_snapshot;
    int draw_login = screen == UI_SCREEN_LOGIN;

    if (draw_login)
        member_login_snapshot(member_login, &login_snapshot);
    lcd_sem_wait(state);
    for (int page = 0; page < lcd_page_count(lcd); ++page)
    {
        int *framebuffer = lcd_page_pixels(lcd, page);

        read_JPEG_file(ui_screen_image(screen), framebuffer);
        draw_back_button(framebuffer, back_button);
        if (draw_login)
            draw_member_login_fields(framebuffer, &login_snapshot);
    }
    (void)lcd_show_page(lcd, 0);
    sem_post(&state->lcd_sem);
    if (draw_login && login_version != NULL)
        *login_version = login_snapshot.version;
}

// 在实时预览和回放期间启用旧的原始触摸线程；静态页面由 LVGL 独占输入。
static int start_dynamic_touch(UiState *state, pthread_t *thread,
                               CameraThreadContext *context, int *running)
{
    pthread_mutex_lock(&state->state_mutex);
    state->touch_running = 1;
    pthread_mutex_unlock(&state->state_mutex);
    if (pthread_create(thread, NULL, touch_thread_main, context) != 0)
    {
        pthread_mutex_lock(&state->state_mutex);
        state->touch_running = 0;
        pthread_mutex_unlock(&state->state_mutex);
        return -1;
    }
    *running = 1;
    return 0;
}

static void stop_dynamic_touch(UiState *state, pthread_t *thread, int *running)
{
    if (!*running)
        return;
    pthread_mutex_lock(&state->state_mutex);
    state->touch_running = 0;
    pthread_mutex_unlock(&state->state_mutex);
    pthread_join(*thread, NULL);
    *running = 0;
}

// 初始化静态 LVGL 页面，并在动态画面期间保留原有 framebuffer 工作流。
int ui_run(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder,
           UiRemoteCommands *remote_commands)
{
    UiState state;
    CameraThreadContext camera_context;
    LiveControls controls;
    RecordIndicator indicator;
    MemberLogin member_login;
    MemberSmsWorker sms_worker;
    LvglRuntime runtime;
    LvglUi lvgl_ui;
    pthread_t touch_thread;
    int touch_thread_running = 0;
    int controls_initialized = 0;
    int indicator_initialized = 0;
    int login_initialized = 0;
    int sms_initialized = 0;
    int runtime_initialized = 0;
    int lvgl_ui_initialized = 0;
    int result = -1;

    memset(&state, 0, sizeof(state));
    memset(&runtime, 0, sizeof(runtime));
    memset(&lvgl_ui, 0, sizeof(lvgl_ui));
    memset(&controls, 0, sizeof(controls));
    memset(&indicator, 0, sizeof(indicator));
    memset(&member_login, 0, sizeof(member_login));
    memset(&sms_worker, 0, sizeof(sms_worker));
    if (sem_init(&state.lcd_sem, 0, 1) == -1 ||
        pthread_mutex_init(&state.state_mutex, NULL) != 0)
        return -1;
    state.screen = UI_SCREEN_MAIN;
    state.parking_guides_enabled = 1;
    if (live_controls_init(&controls) == -1)
        goto cleanup;
    controls_initialized = 1;
    if (record_indicator_init(&indicator) == -1)
        goto cleanup;
    indicator_initialized = 1;
    if (member_login_init(&member_login) == -1)
        goto cleanup;
    login_initialized = 1;
    if (member_sms_worker_init(&sms_worker) == -1)
        goto cleanup;
    sms_initialized = 1;
    if (lvgl_runtime_init(&runtime, lcd, touch) == -1)
    {
        fprintf(stderr, "[UI] LVGL runtime initialization failed\n");
        goto cleanup;
    }
    runtime_initialized = 1;
    if (lvgl_ui_init(&lvgl_ui, &runtime, &member_login, &sms_worker) == -1)
    {
        fprintf(stderr, "[UI] LVGL UI initialization failed\n");
        goto cleanup;
    }
    lvgl_ui_initialized = 1;

    memset(&camera_context, 0, sizeof(camera_context));
    camera_context.lcd = lcd;
    camera_context.touch = touch;
    camera_context.camera = camera;
    camera_context.recorder = recorder;
    camera_context.controls = &controls;
    camera_context.indicator = &indicator;
    camera_context.state = &state;
    camera_context.member_login = &member_login;
    camera_context.sms_worker = &sms_worker;
    lvgl_ui_play_startup(&lvgl_ui);

    for (;;)
    {
        pthread_t camera_thread;
        LvglUiAction action;

        ui_apply_remote_commands(&state, remote_commands);
        if (!ui_is_live(&state) && !ui_is_playback(&state) &&
            !ui_is_file_browser_requested(&state))
        {
            lvgl_ui_process(&lvgl_ui);
            action = lvgl_ui_take_action(&lvgl_ui);
            if (action == LVGL_UI_ACTION_ENTER_LIVE)
                ui_set_live(&state, 1);
            else if (action == LVGL_UI_ACTION_OPEN_FILE_BROWSER)
                ui_request_file_browser(&state);
            usleep(5 * 1000);
            continue;
        }

        if (ui_is_file_browser_requested(&state))
        {
            lvgl_runtime_set_input_enabled(&runtime, 1);
            if (ui_run_file_browser(lcd, touch, recorder, &state, &controls,
                                    &camera_context, &touch_thread,
                                    &touch_thread_running, remote_commands,
                                    &runtime, &lvgl_ui) == -1)
                goto cleanup;
            continue;
        }

        lvgl_runtime_set_input_enabled(&runtime, 0);
        touch_reset_position(touch);
        if (start_dynamic_touch(&state, &touch_thread, &camera_context,
                                &touch_thread_running) == -1)
            goto cleanup;

        if (ui_is_playback(&state))
        {
            playback_run(lcd, &state, &controls, RECORDINGS_DIR, 1,
                         remote_commands);
            stop_dynamic_touch(&state, &touch_thread, &touch_thread_running);
            lvgl_runtime_set_input_enabled(&runtime, 1);
            lvgl_ui_show_main(&lvgl_ui);
            continue;
        }

        if (pthread_create(&camera_thread, NULL, camera_thread_main,
                           &camera_context) != 0)
        {
            ui_set_live(&state, 0);
            stop_dynamic_touch(&state, &touch_thread, &touch_thread_running);
            lvgl_runtime_set_input_enabled(&runtime, 1);
            goto cleanup;
        }
        while (ui_is_live(&state))
        {
            ui_apply_remote_commands(&state, remote_commands);
            usleep(10 * 1000);
        }
        pthread_join(camera_thread, NULL);
        recorder_stop_session(recorder);
        recorder_wait_idle(recorder);

        if (ui_is_file_browser_requested(&state))
        {
            stop_dynamic_touch(&state, &touch_thread, &touch_thread_running);
            lvgl_runtime_set_input_enabled(&runtime, 1);
            continue;
        }
        if (ui_is_playback(&state))
            playback_run(lcd, &state, &controls, RECORDINGS_DIR, 1,
                         remote_commands);
        stop_dynamic_touch(&state, &touch_thread, &touch_thread_running);
        lvgl_runtime_set_input_enabled(&runtime, 1);
        lvgl_ui_show_main(&lvgl_ui);
        touch_reset_position(touch);
    }

cleanup:
    stop_dynamic_touch(&state, &touch_thread, &touch_thread_running);
    if (lvgl_ui_initialized)
        lvgl_ui_destroy(&lvgl_ui);
    if (runtime_initialized)
        lvgl_runtime_destroy(&runtime);
    if (sms_initialized)
        member_sms_worker_destroy(&sms_worker);
    if (login_initialized)
        member_login_destroy(&member_login);
    if (indicator_initialized)
        record_indicator_destroy(&indicator);
    if (controls_initialized)
        live_controls_destroy(&controls);
    pthread_mutex_destroy(&state.state_mutex);
    sem_destroy(&state.lcd_sem);
    return result;
}
//最新修改
