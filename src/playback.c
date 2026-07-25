#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "playback.h"

//最新修改
typedef struct {
    int number;
    char path[256];
} PlaybackItem;

// 判断目录名是否为录像会话时间格式。
static int is_session_directory(const char *name)
{
    static const int separators[] = {4, 7, 10, 13, 16};

    if (strlen(name) != 19 || name[4] != '-' || name[7] != '-' ||
        name[10] != '_' || name[13] != '-' || name[16] != '-')
        return 0;
    for (int i = 0; i < 19; ++i)
    {
        int is_separator = 0;

        for (size_t j = 0; j < sizeof(separators) / sizeof(separators[0]); ++j)
            is_separator |= i == separators[j];
        if (!is_separator && !isdigit((unsigned char)name[i]))
            return 0;
    }
    return 1;
}

// 按录像帧编号排序。
static int compare_playback_items(const void *left, const void *right)
{
    const PlaybackItem *first = left;
    const PlaybackItem *second = right;

    return (first->number > second->number) - (first->number < second->number);
}

// 查找根目录中名称最新的录像会话并加载帧路径。
int playback_load_latest(PlaybackFrames *frames, const char *root_dir)
{
    DIR *root;
    DIR *session;
    struct dirent *entry;
    struct stat status;
    char latest_name[32] = {0};
    char session_path[256];
    PlaybackItem items[JPEG_NUM];
    size_t item_count = 0;

    memset(frames, 0, sizeof(*frames));
    root = opendir(root_dir);
    if (root == NULL)
        return -1;
    while ((entry = readdir(root)) != NULL)
    {
        char candidate[256];

        if (!is_session_directory(entry->d_name))
            continue;
        if (snprintf(candidate, sizeof(candidate), "%s/%s", root_dir,
                     entry->d_name) >= (int)sizeof(candidate) ||
            stat(candidate, &status) == -1 || !S_ISDIR(status.st_mode))
            continue;
        if (latest_name[0] == '\0' || strcmp(entry->d_name, latest_name) > 0)
            memcpy(latest_name, entry->d_name, sizeof(latest_name));
    }
    closedir(root);
    if (latest_name[0] == '\0' ||
        snprintf(session_path, sizeof(session_path), "%s/%s", root_dir,
                 latest_name) >= (int)sizeof(session_path))
        return -1;

    session = opendir(session_path);
    if (session == NULL)
        return -1;
    while ((entry = readdir(session)) != NULL && item_count < JPEG_NUM)
    {
        int number;
        char expected[32];

        if (sscanf(entry->d_name, "frame%d.jpg", &number) != 1 ||
            number < 0 || number >= JPEG_NUM)
            continue;
        snprintf(expected, sizeof(expected), "frame%d.jpg", number);
        if (strcmp(entry->d_name, expected) != 0)
            continue;
        if (snprintf(items[item_count].path, sizeof(items[item_count].path),
                     "%s/%s", session_path, entry->d_name) >=
            (int)sizeof(items[item_count].path))
            continue;
        items[item_count].number = number;
        ++item_count;
    }
    closedir(session);
    if (item_count == 0)
        return -1;

    qsort(items, item_count, sizeof(items[0]), compare_playback_items);
    snprintf(frames->session_dir, sizeof(frames->session_dir), "%s", session_path);
    for (size_t i = 0; i < item_count; ++i)
        snprintf(frames->paths[i], sizeof(frames->paths[i]), "%s", items[i].path);
    frames->count = item_count;
    return 0;
}
//最新修改
