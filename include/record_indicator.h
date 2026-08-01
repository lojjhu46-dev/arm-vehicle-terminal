#ifndef RECORD_INDICATOR_H
#define RECORD_INDICATOR_H

//最新修改
// 预加载的 REC 图标资源。
typedef struct {
    int *pixels;
    int width;
    int height;
} RecordIndicator;

int record_indicator_init(RecordIndicator *indicator); // 加载 REC 图标资源
void record_indicator_destroy(RecordIndicator *indicator); // 释放 REC 图标资源
void record_indicator_draw_dot(int *lcd_mp, int visible); // 绘制或清除红点
void record_indicator_draw_rec(const RecordIndicator *indicator, int *lcd_mp,
                               int visible); // 绘制或清除 REC 标识
//最新修改

#endif
