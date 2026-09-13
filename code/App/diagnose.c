/**
 * diagnose.c —— 多源融合故障诊断实现
 *
 * 六条判据按【优先级】依次判断，命中即返回。
 * 融合三类信号：温度（两路）+ 电流 + 输出占空比。
 */
#include "diagnose.h"
#include "app_config.h"
#include "filter.h"

static uint32_t    s_open_ms   = 0;   /* 断路条件已持续时间 */
static slope_win_t s_r_win;           /* 阻值趋势窗口 */
static slope_win_t s_t_win;           /* 温度趋势窗口 */
static float       s_last_r    = 0.0f;
static float       s_r_slope   = 0.0f;
static float       s_t_slope   = 0.0f;

void diagnose_init(void)
{
    s_open_ms   = 0;
    s_last_r    = 0.0f;
    s_r_slope   = 0.0f;
    s_t_slope   = 0.0f;
    slope_init(&s_r_win);
    slope_init(&s_t_win);
}

float diagnose_get_load_r(void)     { return s_last_r; }
float diagnose_get_r_slope(void)    { return s_r_slope; }
float diagnose_get_temp_slope(void) { return s_t_slope; }

static float per_sec(float slope_per_sample, uint32_t dt_ms)
{
    if (dt_ms == 0) return 0.0f;
    return slope_per_sample / ((float)dt_ms / 1000.0f);
}

fault_code_t diagnose_run(const diag_input_t *in, uint32_t dt_ms)
{
    float duty_ratio = (float)in->duty / (float)CFG_HEATER_MAX_DUTY;

    /* ---- 维持温度趋势窗口（每周期都喂）---- */
    s_t_slope = per_sec(slope_push(&s_t_win, in->t_load), dt_ms);

    /* =================================================================
     * 判据 1：加热回路断路
     *   条件：PWM 有输出（duty>0）但电流近似为 0，且【持续 2 秒】
     *   成因：加热丝断 / 端子松 / 保险丝熔断 / 温控开关动作
     * ================================================================= */
    if (duty_ratio > 0.0f && in->current < CFG_I_OPEN_A) {
        s_open_ms += dt_ms;
        if (s_open_ms >= CFG_I_OPEN_MS) {
            return FAULT_OPEN_CIRCUIT;
        }
    } else {
        s_open_ms = 0;
    }

    /* =================================================================
     * 判据 2：过流 / 短路（最高安全优先级，立即返回）
     * ================================================================= */
    if (in->current > CFG_I_OVER_A) {
        return FAULT_OVERCURRENT;
    }

    /* =================================================================
     * 判据 3：MOS 击穿直通
     *   条件：命令关断（duty=0）但回路仍有电流
     * ================================================================= */
    if (in->duty == 0 && in->current > CFG_I_MOS_SHORT_A) {
        return FAULT_MOS_SHORT;
    }

    /* =================================================================
     * 判据 4：温度传感器漂移
     *   条件：负载与局部两路温度均已达预警区，但偏差超过 5°C
     *   意义：区分"真实过热"与"探头自身失效"
     * ================================================================= */
    if (in->t_load > CFG_WARN_T && in->t_local > CFG_WARN_T) {
        float diff = in->t_load - in->t_local;
        if (diff < 0.0f) diff = -diff;
        if (diff > CFG_T_DRIFT_C) {
            return FAULT_SENSOR_DRIFT;
        }
    }

    /* =================================================================
     * 判据 5：散热异常（一阶热模型残差）
     *
     *   预期温升速率 = ( Pmax×占空比 - k·(T - Tamb) ) / C
     *   残差 = 实测温升速率 - 预期温升速率
     *   残差大于阈值说明实际升温比模型预测快，散热通道出了问题：
     *   风道堵塞、环境温度失控、或者负载被隔热材料包住。
     *
     *   用电流推算的功率作基准，而不是给温升速率设固定阈值。
     *   后者在物理上不成立：升温速率取决于加热功率和负载热容，
     *   不是可以拍脑袋定一个常数的事情。
     * ================================================================= */
    if (duty_ratio > 0.1f && in->current > 0.2f && in->current < CFG_I_OVER_A) {
        float p_w       = CFG_HEATER_PMAX_W * duty_ratio;
        float expected  = (p_w - CFG_THERM_K_W_PER_K * (in->t_load - in->t_env))
                          / CFG_THERM_C_J_PER_K;
        float residual  = s_t_slope - expected;

        if (residual > CFG_RESIDUAL_ABN_C_PER_S) {
            return FAULT_THERMAL_ABNORMAL;
        }
    }

    /* =================================================================
     * 判据 6：元件老化趋势预警
     *   条件：由 R = U / I 算出的负载阻值在滑动窗口内持续上升
     *   意义：水泥电阻长期高温会阻值漂移 → 健康度退化预警
     *   注意：只在"确实在导通且有电流"时才计算 R
     * ================================================================= */
    if (duty_ratio > 0.0f && in->current > CFG_I_OPEN_A && in->bus_voltage > 0.5f) {
        float r = in->bus_voltage / in->current;
        s_last_r = r;
        s_r_slope = per_sec(slope_push(&s_r_win, r), dt_ms);
        if (s_r_slope > CFG_R_TREND_OHM_PER_S) {
            return FAULT_AGING_WARNING;
        }
    } else {
        s_r_slope = 0.0f;
    }

    return FAULT_NONE;
}

const char *fault_name(fault_code_t f)
{
    switch (f) {
        case FAULT_NONE:             return "无故障";
        case FAULT_OPEN_CIRCUIT:     return "加热回路断路";
        case FAULT_OVERCURRENT:      return "过流 / 短路";
        case FAULT_MOS_SHORT:        return "MOS 击穿直通";
        case FAULT_SENSOR_DRIFT:     return "温度传感器漂移";
        case FAULT_THERMAL_ABNORMAL: return "散热异常";
        case FAULT_AGING_WARNING:    return "元件老化趋势预警";
        default:                     return "未知";
    }
}
