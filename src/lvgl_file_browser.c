#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <lvgl.h>

#include "app_config.h"
#include "jpeg_codec.h"
#include "lvgl_file_browser.h"

#define BROWSER_PATH_SIZE 256
#define BROWSER_MAX_ENTRIES 128
#define BROWSER_DRAW_BUFFER_BYTES (LCD_WIDTH * LCD_HEIGHT * 3)
#define BROWSER_HEADER_HEIGHT 58
#define BROWSER_STATUS_Y 450
#define BROWSER_LIST_Y 60
#define BROWSER_LIST_HEIGHT 384
#define BROWSER_ROW_HEIGHT 44

typedef struct BrowserUi BrowserUi;

typedef struct {
    BrowserUi *ui;
    char name[NAME_MAX + 1];
    int is_directory;
} BrowserEntry;

typedef struct {
    Lcd *lcd;
    Touch *touch;
    int x;
    int y;
} BrowserInput;

struct BrowserUi {
    Lcd *lcd;
    LvglBrowserSelection *selection;
    char root_dir[BROWSER_PATH_SIZE];
    char current_dir[BROWSER_PATH_SIZE];
    BrowserEntry entries[BROWSER_MAX_ENTRIES];
    size_t entry_count;
    int done;
    int photo_visible;
    int *photo_pixels;
    uint8_t *photo_data;
    size_t photo_data_size;
    int photo_width;
    int photo_height;
    lv_image_dsc_t photo_descriptor;
    lv_obj_t *screen;
    lv_obj_t *back_button;
    lv_obj_t *path_label;
    lv_obj_t *list;
    lv_obj_t *photo_panel;
    lv_obj_t *status_label;
};

static void browser_entry_event_cb(lv_event_t *event);

// 返回单调时钟毫秒值，供 LVGL 驱动动画与定时器。
static uint32_t browser_tick_get(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint32_t)(now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL);
}

// 判断规范化路径是否仍在允许浏览的根目录内。
static int path_is_within(const char *root_dir, const char *candidate)
{
    size_t root_length = strlen(root_dir);

    if (strncmp(root_dir, candidate, root_length) != 0)
        return 0;
    return candidate[root_length] == '\0' || candidate[root_length] == '/';
}

// 复制路径并拒绝超过浏览器固定缓冲区的输入。
static int copy_path(char destination[BROWSER_PATH_SIZE], const char *source)
{
    if (strlen(source) >= BROWSER_PATH_SIZE)
        return -1;
    strcpy(destination, source);
    return 0;
}

// 解析真实路径并拦截符号链接或路径跳转到根目录外的情况。
static int resolve_existing_path(const BrowserUi *ui, const char *candidate,
                                 char resolved[BROWSER_PATH_SIZE])
{
    char absolute[PATH_MAX];

    if (realpath(candidate, absolute) == NULL ||
        !path_is_within(ui->root_dir, absolute))
        return -1;
    return copy_path(resolved, absolute);
}

// 判断普通文件名是否为可预览的 JPEG。
static int is_jpeg_name(const char *name)
{
    size_t length = strlen(name);

    if (length < 4 || name[0] == '.')
        return 0;
    if (strcasecmp(name + length - 4, ".jpg") == 0)
        return 1;
    return length >= 5 && strcasecmp(name + length - 5, ".jpeg") == 0;
}

// 校验录像会话目录的 YYYY-MM-DD_HH-MM-SS 命名格式。
static int is_session_name(const char *name)
{
    static const int separators[] = {4, 7, 10, 13, 16};
    size_t length = strlen(name);

    if (length != 19 || name[4] != '-' || name[7] != '-' ||
        name[10] != '_' || name[13] != '-' || name[16] != '-')
        return 0;
    for (size_t i = 0; i < length; ++i)
    {
        int separator = 0;

        for (size_t j = 0; j < sizeof(separators) / sizeof(separators[0]); ++j)
            separator |= (int)i == separators[j];
        if (!separator && !isdigit((unsigned char)name[i]))
            return 0;
    }
    return 1;
}

// 校验录像帧的 frame<number>.jpg 命名及编号范围。
static int is_frame_name(const char *name)
{
    int number;
    char expected[32];

    if (sscanf(name, "frame%d.jpg", &number) != 1 || number < 0 ||
        number >= JPEG_NUM)
        return 0;
    if (snprintf(expected, sizeof(expected), "frame%d.jpg", number) >=
        (int)sizeof(expected))
        return 0;
    return strcmp(name, expected) == 0;
}

// 检查录像会话中是否至少有一帧位于受限根目录内。
static int session_has_frames(const BrowserUi *ui, const char *session_dir)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir(session_dir);
    if (directory == NULL)
        return 0;
    while ((entry = readdir(directory)) != NULL)
    {
        char candidate[PATH_MAX];
        struct stat status;
        char resolved[BROWSER_PATH_SIZE];

        if (!is_frame_name(entry->d_name) ||
            snprintf(candidate, sizeof(candidate), "%s/%s", session_dir,
                     entry->d_name) >= (int)sizeof(candidate) ||
            lstat(candidate, &status) == -1 || !S_ISREG(status.st_mode) ||
            resolve_existing_path(ui, candidate, resolved) == -1)
            continue;
        closedir(directory);
        return 1;
    }
    closedir(directory);
    return 0;
}

// 排序目录项：目录优先，其次按名称不区分大小写排序。
static int compare_entries(const void *left, const void *right)
{
    const BrowserEntry *first = left;
    const BrowserEntry *second = right;

    if (first->is_directory != second->is_directory)
        return second->is_directory - first->is_directory;
    return strcasecmp(first->name, second->name);
}

// 更新底部状态栏，空指针表示清空文本。
static void browser_set_status(BrowserUi *ui, const char *message)
{
    lv_label_set_text(ui->status_label, message != NULL ? message : "");
}

// 释放当前照片的原始像素和 LVGL RGB 数据。
static void browser_free_photo(BrowserUi *ui)
{
    free(ui->photo_pixels);
    free(ui->photo_data);
    ui->photo_pixels = NULL;
    ui->photo_data = NULL;
    ui->photo_data_size = 0;
    ui->photo_width = 0;
    ui->photo_height = 0;
    memset(&ui->photo_descriptor, 0, sizeof(ui->photo_descriptor));
}

// 将当前目录显示到顶部路径标签。
static void browser_set_path_label(BrowserUi *ui)
{
    lv_label_set_text(ui->path_label, ui->current_dir);
}

// 切回文件列表，并根据是否在根目录更新返回按钮状态。
static void browser_show_list(BrowserUi *ui)
{
    browser_free_photo(ui);
    ui->photo_visible = 0;
    lv_obj_add_flag(ui->photo_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->list, LV_OBJ_FLAG_HIDDEN);
    browser_set_path_label(ui);
    browser_set_status(ui, NULL);
    if (strcmp(ui->current_dir, ui->root_dir) == 0)
        lv_obj_add_state(ui->back_button, LV_STATE_DISABLED);
    else
        lv_obj_clear_state(ui->back_button, LV_STATE_DISABLED);
}

// 解码 JPEG 并转换为 LVGL RGB888 所需的 BGR 字节序。
static int browser_load_photo(BrowserUi *ui, const char *path)
{
    size_t pixel_count;

    if (read_JPEG_image(path, &ui->photo_pixels, &ui->photo_width,
                        &ui->photo_height) == -1 || ui->photo_width <= 0 ||
        ui->photo_height <= 0)
    {
        browser_free_photo(ui);
        browser_set_status(ui, "Unable to decode JPEG");
        return -1;
    }
    pixel_count = (size_t)ui->photo_width * (size_t)ui->photo_height;
    if (pixel_count > SIZE_MAX / 3 || ui->photo_width > UINT16_MAX / 3 ||
        ui->photo_height > UINT16_MAX || pixel_count > UINT32_MAX / 3)
    {
        browser_free_photo(ui);
        browser_set_status(ui, "Image is too large");
        return -1;
    }
    ui->photo_data_size = pixel_count * 3;
    ui->photo_data = malloc(ui->photo_data_size);
    if (ui->photo_data == NULL)
    {
        browser_free_photo(ui);
        browser_set_status(ui, "Not enough memory");
        return -1;
    }
    for (size_t i = 0; i < pixel_count; ++i)
    {
        unsigned int pixel = (unsigned int)ui->photo_pixels[i];

        /* LVGL RGB888 stores B, G, R bytes; the raw UI stores 0x00RRGGBB. */
        ui->photo_data[i * 3 + 0] = (uint8_t)(pixel & 0xffU);
        ui->photo_data[i * 3 + 1] = (uint8_t)((pixel >> 8) & 0xffU);
        ui->photo_data[i * 3 + 2] = (uint8_t)((pixel >> 16) & 0xffU);
    }
    free(ui->photo_pixels);
    ui->photo_pixels = NULL;
    return 0;
}

// 创建照片控件，按可视区域等比例缩放并显示。
static void browser_show_photo(BrowserUi *ui, const char *path)
{
    lv_obj_t *image;
    int scale;

    if (browser_load_photo(ui, path) == -1)
        return;
    memset(&ui->photo_descriptor, 0, sizeof(ui->photo_descriptor));
    ui->photo_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    ui->photo_descriptor.header.cf = LV_COLOR_FORMAT_RGB888;
    ui->photo_descriptor.header.w = (uint16_t)ui->photo_width;
    ui->photo_descriptor.header.h = (uint16_t)ui->photo_height;
    ui->photo_descriptor.header.stride = (uint16_t)(ui->photo_width * 3);
    ui->photo_descriptor.data_size = (uint32_t)ui->photo_data_size;
    ui->photo_descriptor.data = ui->photo_data;
    image = lv_image_create(ui->photo_panel);
    lv_image_set_src(image, &ui->photo_descriptor);
    scale = 256;
    if (ui->photo_width > 780 || ui->photo_height > 380)
    {
        int width_scale = 780 * 256 / ui->photo_width;
        int height_scale = 380 * 256 / ui->photo_height;

        scale = width_scale < height_scale ? width_scale : height_scale;
        if (scale > 256)
            scale = 256;
    }
    if (scale < 1)
        scale = 1;
    lv_image_set_scale(image, (uint32_t)scale);
    lv_obj_center(image);
    lv_obj_add_flag(ui->list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->photo_panel, LV_OBJ_FLAG_HIDDEN);
    ui->photo_visible = 1;
    lv_label_set_text(ui->path_label, path);
    lv_obj_clear_state(ui->back_button, LV_STATE_DISABLED);
    browser_set_status(ui, NULL);
}

// 扫描当前目录，过滤不安全项后生成可滚动的 LVGL 列表。
static void browser_show_directory(BrowserUi *ui)
{
    DIR *directory;
    struct dirent *entry;

    ui->entry_count = 0;
    directory = opendir(ui->current_dir);
    if (directory == NULL)
    {
        browser_set_status(ui, "Unable to open directory");
        return;
    }
    while ((entry = readdir(directory)) != NULL &&
           ui->entry_count < BROWSER_MAX_ENTRIES)
    {
        char candidate[PATH_MAX];
        char resolved[BROWSER_PATH_SIZE];
        struct stat status;
        BrowserEntry *item;

        if (entry->d_name[0] == '.' ||
            snprintf(candidate, sizeof(candidate), "%s/%s", ui->current_dir,
                     entry->d_name) >= (int)sizeof(candidate) ||
            lstat(candidate, &status) == -1 || S_ISLNK(status.st_mode) ||
            resolve_existing_path(ui, candidate, resolved) == -1)
            continue;
        if (!S_ISDIR(status.st_mode) &&
            !(S_ISREG(status.st_mode) && is_jpeg_name(entry->d_name)))
            continue;
        if (strlen(entry->d_name) > NAME_MAX)
            continue;
        item = &ui->entries[ui->entry_count++];
        item->ui = ui;
        strcpy(item->name, entry->d_name);
        item->is_directory = S_ISDIR(status.st_mode);
    }
    closedir(directory);
    qsort(ui->entries, ui->entry_count, sizeof(ui->entries[0]), compare_entries);

    lv_obj_clean(ui->list);
    lv_obj_set_height(ui->list, BROWSER_LIST_HEIGHT);
    for (size_t i = 0; i < ui->entry_count; ++i)
    {
        lv_obj_t *button = lv_button_create(ui->list);
        lv_obj_t *label = lv_label_create(button);
        int y = (int)i * BROWSER_ROW_HEIGHT;

        lv_obj_set_size(button, LCD_WIDTH - 24, BROWSER_ROW_HEIGHT - 4);
        lv_obj_set_pos(button, 8, y);
        lv_obj_set_style_radius(button, 2, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x263746), 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x3f6075), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(0x476276), 0);
        lv_label_set_text_fmt(label, "%s  %s", ui->entries[i].is_directory ?
                              "DIR" : "JPG", ui->entries[i].name);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_add_event_cb(button, browser_entry_event_cb, LV_EVENT_CLICKED,
                            &ui->entries[i]);
    }
    lv_obj_set_height(ui->list, BROWSER_LIST_HEIGHT);
    browser_show_list(ui);
}

// 从照片预览返回列表，或从子目录返回父目录。
static void browser_go_parent(BrowserUi *ui)
{
    char *slash;

    if (ui->photo_visible)
    {
        lv_obj_clean(ui->photo_panel);
        browser_show_list(ui);
        return;
    }
    if (strcmp(ui->current_dir, ui->root_dir) == 0)
        return;
    slash = strrchr(ui->current_dir, '/');
    if (slash == NULL || slash == ui->current_dir)
        copy_path(ui->current_dir, ui->root_dir);
    else
    {
        *slash = '\0';
        if (!path_is_within(ui->root_dir, ui->current_dir))
            copy_path(ui->current_dir, ui->root_dir);
    }
    browser_show_directory(ui);
}

// 处理目录项点击：进入目录、启动录像回放或打开照片。
static void browser_entry_event_cb(lv_event_t *event)
{
    BrowserEntry *entry = lv_event_get_user_data(event);
    BrowserUi *ui = entry->ui;
    char candidate[PATH_MAX];
    char resolved[BROWSER_PATH_SIZE];

    if (snprintf(candidate, sizeof(candidate), "%s/%s", ui->current_dir,
                 entry->name) >= (int)sizeof(candidate) ||
        resolve_existing_path(ui, candidate, resolved) == -1)
    {
        browser_set_status(ui, "Path is outside storage root");
        return;
    }
    if (entry->is_directory)
    {
        if (is_session_name(entry->name) && session_has_frames(ui, resolved))
        {
            ui->selection->result = LVGL_BROWSER_PLAYBACK;
            copy_path(ui->selection->path, resolved);
            ui->done = 1;
            return;
        }
        if (copy_path(ui->current_dir, resolved) == -1)
        {
            browser_set_status(ui, "Path is too long");
            return;
        }
        browser_show_directory(ui);
        return;
    }
    browser_show_photo(ui, resolved);
}

// 转发 LVGL 返回按钮事件。
static void browser_back_event_cb(lv_event_t *event)
{
    browser_go_parent(lv_event_get_user_data(event));
}

// 标记浏览器结束并返回主 UI。
static void browser_exit_event_cb(lv_event_t *event)
{
    BrowserUi *ui = lv_event_get_user_data(event);

    ui->selection->result = LVGL_BROWSER_EXIT;
    ui->done = 1;
}

// 将 LVGL RGB888 脏区域转换为 framebuffer 像素并翻页显示。
static void browser_flush_cb(lv_display_t *display, const lv_area_t *area,
                             uint8_t *color_p)
{
    BrowserUi *ui = lv_display_get_user_data(display);
    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;

    if (lcd_begin_frame(ui->lcd) == 0)
    {
        for (int y = 0; y < height; ++y)
        {
            const uint8_t *source = color_p + (size_t)y * (size_t)width * 3;
            int *target = ui->lcd->pixels +
                          (area->y1 + y) * LCD_WIDTH + area->x1;

            for (int x = 0; x < width; ++x)
            {
                /* LVGL's RGB888 renderer emits BGR bytes on this target. */
                target[x] = source[x * 3 + 2] << 16 |
                            source[x * 3 + 1] << 8 | source[x * 3 + 0];
            }
        }
        (void)lcd_present(ui->lcd);
    }
    lv_display_flush_ready(display);
}

// 将项目触摸驱动的坐标和按压状态适配为 LVGL 指针输入。
static void browser_input_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    BrowserInput *input = lv_indev_get_user_data(indev);
    int pressed;

    if (touch_read_pointer(input->touch, &input->x, &input->y, &pressed) == -1)
    {
        data->point.x = input->x;
        data->point.y = input->y;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->point.x = input->x;
    data->point.y = input->y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// 创建文件列表、图片预览、路径栏和操作按钮。
static void browser_create_ui(BrowserUi *ui)
{
    lv_obj_t *label;
    lv_obj_t *exit_button;

    ui->screen = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(ui->screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(ui->screen, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui->screen, 0, 0);
    lv_obj_set_style_pad_all(ui->screen, 0, 0);

    ui->back_button = lv_button_create(ui->screen);
    lv_obj_set_size(ui->back_button, 92, 46);
    lv_obj_set_pos(ui->back_button, 8, 6);
    label = lv_label_create(ui->back_button);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);
    lv_obj_add_event_cb(ui->back_button, browser_back_event_cb, LV_EVENT_CLICKED, ui);

    exit_button = lv_button_create(ui->screen);
    lv_obj_set_size(exit_button, 92, 46);
    lv_obj_set_pos(exit_button, LCD_WIDTH - 100, 6);
    label = lv_label_create(exit_button);
    lv_label_set_text(label, "Exit");
    lv_obj_center(label);
    lv_obj_add_event_cb(exit_button, browser_exit_event_cb, LV_EVENT_CLICKED, ui);

    ui->path_label = lv_label_create(ui->screen);
    lv_obj_set_width(ui->path_label, LCD_WIDTH - 220);
    lv_obj_set_pos(ui->path_label, 110, 20);
    lv_label_set_long_mode(ui->path_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(ui->path_label, lv_color_white(), 0);

    ui->list = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->list, LCD_WIDTH, BROWSER_LIST_HEIGHT);
    lv_obj_set_pos(ui->list, 0, BROWSER_LIST_Y);
    lv_obj_set_style_bg_color(ui->list, lv_color_hex(0x17232d), 0);
    lv_obj_set_style_bg_opa(ui->list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui->list, 0, 0);
    lv_obj_set_style_radius(ui->list, 0, 0);
    lv_obj_set_style_pad_all(ui->list, 0, 0);
    lv_obj_set_scroll_dir(ui->list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui->list, LV_SCROLLBAR_MODE_AUTO);

    ui->photo_panel = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->photo_panel, LCD_WIDTH, LCD_HEIGHT - BROWSER_HEADER_HEIGHT);
    lv_obj_set_pos(ui->photo_panel, 0, BROWSER_HEADER_HEIGHT);
    lv_obj_set_style_bg_color(ui->photo_panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui->photo_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui->photo_panel, 0, 0);
    lv_obj_set_style_radius(ui->photo_panel, 0, 0);

    ui->status_label = lv_label_create(ui->screen);
    lv_obj_set_width(ui->status_label, LCD_WIDTH - 20);
    lv_obj_set_pos(ui->status_label, 10, BROWSER_STATUS_Y);
    lv_obj_set_style_text_color(ui->status_label, lv_color_hex(0xffb36b), 0);

    browser_show_directory(ui);
    lv_screen_load(ui->screen);
}

// 运行受限于 root_dir 的模态文件浏览器，直到退出或选择回放。
int lvgl_file_browser_run(Lcd *lcd, Touch *touch, const char *root_dir,
                          LvglBrowserSelection *selection,
                          LvglBrowserInterrupt interrupt,
                          void *interrupt_data)
{
    BrowserUi ui;
    BrowserInput input;
    lv_display_t *display;
    lv_indev_t *indev;
    uint8_t *draw_buffer;
    char resolved_root[BROWSER_PATH_SIZE];
    struct stat root_status;
    int result = -1;

    if (lcd == NULL || touch == NULL || root_dir == NULL || selection == NULL ||
        realpath(root_dir, resolved_root) == NULL ||
        strlen(resolved_root) >= BROWSER_PATH_SIZE ||
        stat(resolved_root, &root_status) == -1 || !S_ISDIR(root_status.st_mode))
        return -1;
    memset(selection, 0, sizeof(*selection));
    selection->result = LVGL_BROWSER_EXIT;
    memset(&ui, 0, sizeof(ui));
    ui.lcd = lcd;
    ui.selection = selection;
    copy_path(ui.root_dir, resolved_root);
    copy_path(ui.current_dir, resolved_root);
    memset(&input, 0, sizeof(input));
    input.lcd = lcd;
    input.touch = touch;
    draw_buffer = malloc(BROWSER_DRAW_BUFFER_BYTES);
    if (draw_buffer == NULL)
        return -1;

    lv_init();
    lv_tick_set_cb(browser_tick_get);
    display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    if (display == NULL)
        goto cleanup_lvgl;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, draw_buffer, NULL, BROWSER_DRAW_BUFFER_BYTES,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, browser_flush_cb);
    lv_display_set_user_data(display, &ui);
    browser_create_ui(&ui);

    /* 先渲染首帧，再安装可能阻塞读取触摸设备的输入回调。 */
    lv_refr_now(display);
    indev = lv_indev_create();
    if (indev == NULL)
        goto cleanup_lvgl;
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_user_data(indev, &input);
    lv_indev_set_read_cb(indev, browser_input_read_cb);
    while (!ui.done)
    {
        if (interrupt != NULL && interrupt(interrupt_data))
        {
            ui.selection->result = LVGL_BROWSER_EXIT;
            ui.done = 1;
            continue;
        }
        lv_timer_handler();
        usleep(5 * 1000);
    }
    result = 0;

cleanup_lvgl:
    lv_deinit();
    free(draw_buffer);
    return result;
}
