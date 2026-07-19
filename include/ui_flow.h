#ifndef UI_FLOW_H
#define UI_FLOW_H

#include "camera.h"
#include "lcd.h"
#include "recorder.h"
#include "touch.h"

void show_startup_animation(Lcd *lcd);
int ui_run(Lcd *lcd, Touch *touch, Camera *camera, Recorder *recorder);

#endif
