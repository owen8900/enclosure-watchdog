/**
 * heater.c —— 1Hz 时间比例加热控制实现
 *
 * 为什么不用 kHz PWM：
 *   1) INA219 在脉冲电流下采样会失真 → 必须在导通期稳定采样
 *   2) MOS 开关损耗趋近于零
 *   3) 不污染 I2C / SPI 总线
 *   4) 10W 水泥电阻热时间常数为数十秒，根本不需要高频调制
 */
#include "heater.h"
#include "app_config.h"

static pwm_set_fn s_fn      = 0;
static float      s_target  = CFG_HEATER_TARGET;
static uint8_t    s_enabled = 0;
static uint32_t   s_phase   = 0;      /* 当前处于 1s 周期内的毫秒位置 */
static uint16_t   s_duty    = 0;      /* 目标占空比 0..10000 */
static uint8_t    s_on      = 0;

void heater_init(pwm_set_fn fn)
{
    s_fn      = fn;
    s_target  = CFG_HEATER_TARGET;
    s_enabled = 0;
    s_phase   = 0;
    s_duty    = 0;
    s_on      = 0;
    if (s_fn) s_fn(0);
}

void heater_set_target(float t_c)        { s_target = t_c; }
float heater_get_target(void)            { return s_target; }
uint8_t heater_is_enabled(void)          { return s_enabled; }
uint16_t heater_get_duty(void)           { return s_duty; }
uint8_t heater_is_on(void)               { return s_on; }

void heater_set_enable(uint8_t en)
{
    s_enabled = en ? 1 : 0;
    if (!s_enabled) {
        s_duty = 0;
        s_on = 0;
        if (s_fn) s_fn(0);
    }
}

void heater_tick(uint32_t dt_ms, float t_load)
{
    float    err;
    uint32_t on_ms;

    /* ---- 禁止加热：安全侧动作，直接断输出 ---- */
    if (!s_enabled) {
        s_duty = 0;
        s_on = 0;
        if (s_fn) s_fn(0);
        return;
    }

    /* ---- 按温度误差计算目标占空比（简单比例控制）---- */
    err = s_target - t_load;
    if (err <= 0.0f) {
        s_duty = 0;                                        /* 已到温，停 */
    } else if (err >= CFG_HEATER_FULL_ERR) {
        s_duty = CFG_HEATER_MAX_DUTY;                      /* 差得远，全功率 */
    } else {
        s_duty = (uint16_t)((err / CFG_HEATER_FULL_ERR) * (float)CFG_HEATER_MAX_DUTY);
    }

    /* ---- 1Hz 时间比例：在 1 秒周期内，前 on_ms 毫秒导通 ---- */
    s_phase = (s_phase + dt_ms) % CFG_HEATER_PERIOD_MS;
    on_ms   = ((uint32_t)s_duty * CFG_HEATER_PERIOD_MS) / CFG_HEATER_MAX_DUTY;

    s_on = (s_phase < on_ms) ? 1 : 0;
    if (s_fn) s_fn(s_on ? CFG_HEATER_MAX_DUTY : 0);
}
