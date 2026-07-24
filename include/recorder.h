#ifndef RECORDER_H
#define RECORDER_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "jpeglib.h"

typedef struct {
    char pathname[256];
    char dirbuffer[256];
    int cnt;
    int CNT;
    unsigned int photo_count;
    int recording;
} Recorder;

void recorder_init(Recorder *recorder);
int recorder_prepare_session(Recorder *recorder, const char *root_dir, time_t timestamp);
int recorder_start_session(Recorder *recorder, const char *root_dir);
void recorder_stop_session(Recorder *recorder);
int recorder_save_frame(Recorder *recorder, const JSAMPLE *image_buffer);
int recorder_save_photo(Recorder *recorder, const char *photo_dir,
                        const JSAMPLE *image_buffer);
const char *recorder_last_path(const Recorder *recorder);

#endif
