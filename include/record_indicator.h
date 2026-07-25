#ifndef RECORD_INDICATOR_H
#define RECORD_INDICATOR_H

//最新修改
// 预加载的 REC 图标资源。
typedef struct {
    int *pixels;
    int width;
    int height;
} RecordIndicator;

int record_indicator_init(RecordIndicator *indicator);
void record_indicator_destroy(RecordIndicator *indicator);
void record_indicator_draw_dot(int *lcd_mp, int visible);
void record_indicator_draw_rec(const RecordIndicator *indicator, int *lcd_mp,
                               int visible);
//最新修改

#endif
