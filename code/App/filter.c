/**
 * filter.c —— 滤波与趋势分析实现
 */
#include "filter.h"

/* ---------------------------------------------------------------
 * 3 点中值：三步比较网络，无需数组
 * --------------------------------------------------------------- */
float filter_median3(float a, float b, float c)
{
    float t;
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;
}

/* ---------------------------------------------------------------
 * 滑动平均
 * --------------------------------------------------------------- */
void mavg_init(mavg_t *m)
{
    uint8_t i;
    m->idx = 0;
    m->cnt = 0;
    for (i = 0; i < CFG_MAVG_N; i++) m->buf[i] = 0.0f;
}

float mavg_push(mavg_t *m, float x)
{
    float   sum = 0.0f;
    uint8_t i;

    m->buf[m->idx] = x;
    m->idx = (uint8_t)((m->idx + 1) % CFG_MAVG_N);
    if (m->cnt < CFG_MAVG_N) m->cnt++;

    for (i = 0; i < m->cnt; i++) sum += m->buf[i];
    return sum / (float)m->cnt;
}

/* ---------------------------------------------------------------
 * 滑动窗口最小二乘斜率（把窗口内样本按时间先后排好再做回归）
 * --------------------------------------------------------------- */
void slope_init(slope_win_t *s)
{
    uint8_t i;
    s->idx = 0;
    s->cnt = 0;
    for (i = 0; i < CFG_R_WIN_N; i++) s->buf[i] = 0.0f;
}

float slope_push(slope_win_t *s, float x)
{
    float   xs[CFG_R_WIN_N];
    float   sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f, den, n_f;
    int     n, i, k;

    s->buf[s->idx] = x;
    s->idx = (uint8_t)((s->idx + 1) % CFG_R_WIN_N);
    if (s->cnt < CFG_R_WIN_N) s->cnt++;

    n = (int)s->cnt;
    if (n < 3) return 0.0f;

    /* 按时间从旧到新取出 */
    for (i = 0; i < n; i++) {
        if (s->cnt < CFG_R_WIN_N) k = i;                         /* 未绕圈 */
        else                      k = (s->idx + i) % CFG_R_WIN_N; /* 已绕圈 */
        xs[i] = s->buf[k];
    }

    for (i = 0; i < n; i++) {
        sx  += (float)i;
        sy  += xs[i];
        sxx += (float)i * (float)i;
        sxy += (float)i * xs[i];
    }
    n_f = (float)n;
    den = n_f * sxx - sx * sx;
    if (den == 0.0f || den == -0.0f) return 0.0f;

    return (n_f * sxy - sx * sy) / den;
}
