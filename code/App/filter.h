/**
 * filter.h —— 数据滤波与趋势分析（纯数学，无硬件依赖）
 */
#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include "app_config.h"

/* 3 点中值滤波：滤掉单点毛刺 */
float filter_median3(float a, float b, float c);

/* 滑动平均 */
typedef struct {
    float   buf[CFG_MAVG_N];
    uint8_t idx;
    uint8_t cnt;
} mavg_t;

void  mavg_init(mavg_t *m);
float mavg_push(mavg_t *m, float x);

/* 滑动窗口最小二乘斜率 */
typedef struct {
    float   buf[CFG_R_WIN_N];
    uint8_t idx;
    uint8_t cnt;
} slope_win_t;

void  slope_init(slope_win_t *s);
float slope_push(slope_win_t *s, float x);   /* 返回「单位/样本」的斜率 */

#endif /* FILTER_H */
