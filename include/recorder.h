#ifndef RECORDER_H
#define RECORDER_H

#include <stddef.h>
#include <stdio.h>

#include "jpeglib.h"

typedef struct {
    char pathname[256];
    char dirbuffer[256];
    int cnt;
    int CNT;
} Recorder;

void recorder_init(Recorder *recorder);
void recorder_save_frame(Recorder *recorder, int touch_x, const JSAMPLE *image_buffer);
const char *recorder_last_path(const Recorder *recorder);

#endif
