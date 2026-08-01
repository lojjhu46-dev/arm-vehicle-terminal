#ifndef PLAYBACK_H
#define PLAYBACK_H

#include <stddef.h>

#include "app_config.h"

//最新修改
// 最新录像会话中按帧号排序的 JPEG 路径。
typedef struct {
    char session_dir[256];
    char paths[JPEG_NUM][256];
    size_t count;
} PlaybackFrames;

int playback_load_latest(PlaybackFrames *frames, const char *root_dir); // 加载最新录像会话的帧路径
//最新修改

#endif
