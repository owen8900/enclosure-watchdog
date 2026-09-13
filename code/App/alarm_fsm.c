/**
 * alarm_fsm.c —— 三级告警状态机实现
 *
 * 状态迁移规则：
 *   向上（升温）：NORMAL→WARN→ALARM→EMERGENCY，均需"持续 3 秒"去抖
 *                但 EMERGENCY 为安全侧动作，【立即】触发不去抖
 *   向下（降温）：WARN→NORMAL 需低于 40°C，ALARM→WARN 需低于 55°C（迟滞）
 *   硬件故障：   进入 ST_FAULT 并【锁存】，禁止自动恢复
 */
#include "alarm_fsm.h"
#include "app_config.h"

static alarm_state_t s_state = ST_NORMAL;
static uint32_t      s_debounce_ms = 0;
static uint8_t       s_latched = 0;

void alarm_fsm_init(void)
{
    s_state = ST_NORMAL;
    s_debounce_ms = 0;
    s_latched = 0;
}

alarm_state_t alarm_fsm_get_state(void)      { return s_state; }
uint8_t       alarm_fsm_is_latched(void)     { return s_latched; }

void alarm_fsm_latch_fault(void)
{
    s_latched = 1;
    s_state = ST_FAULT;
    s_debounce_ms = 0;
}

void alarm_fsm_clear(void)
{
    s_latched = 0;
    s_state = ST_NORMAL;
    s_debounce_ms = 0;
}

alarm_state_t alarm_fsm_update(const alarm_input_t *in, uint32_t dt_ms)
{
    float          t;
    alarm_state_t  want;

    /* ---- 0. 已锁存：只能人工复位 ---- */
    if (s_latched) {
        s_state = ST_FAULT;
        return s_state;
    }

    t = in->t_load;

    /* ---- 1. 紧急：安全侧动作，立即触发，不去抖 ---- */
    if (t > CFG_EMERG_T) {
        if (s_state != ST_EMERGENCY) {
            s_state = ST_EMERGENCY;
            s_debounce_ms = 0;
        }
        return s_state;
    }

    /* ---- 2. 向上迁移：需持续去抖确认 ---- */
    want = s_state;
    if      (s_state == ST_NORMAL && t > CFG_WARN_T)   want = ST_WARN;
    else if (s_state == ST_WARN   && t > CFG_ALARM_T)  want = ST_ALARM;
    else if (s_state == ST_ALARM  && t > CFG_EMERG_T)  want = ST_EMERGENCY;

    if (want != s_state) {
        s_debounce_ms += dt_ms;
        if (s_debounce_ms >= CFG_DEBOUNCE_MS) {
            s_state = want;
            s_debounce_ms = 0;
        }
    } else {
        s_debounce_ms = 0;   /* 条件不满足则计时清零，实现"持续"语义 */
    }

    /* ---- 3. 向下恢复：迟滞，不去抖（安全侧可以直接降级）---- */
    if      (s_state == ST_WARN      && t < CFG_WARN_CLEAR_T)  s_state = ST_NORMAL;
    else if (s_state == ST_ALARM     && t < CFG_ALARM_CLEAR_T) s_state = ST_WARN;
    else if (s_state == ST_EMERGENCY && t < CFG_ALARM_T)       s_state = ST_ALARM;

    return s_state;
}

const char *alarm_state_name(alarm_state_t s)
{
    switch (s) {
        case ST_NORMAL:    return "NORMAL   (正常)";
        case ST_WARN:      return "WARN     (预警)";
        case ST_ALARM:     return "ALARM    (告警)";
        case ST_EMERGENCY: return "EMERGENCY(紧急)";
        case ST_FAULT:     return "FAULT    (故障锁存)";
        default:           return "UNKNOWN";
    }
}
