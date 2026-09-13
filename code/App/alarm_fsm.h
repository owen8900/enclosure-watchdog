/**
 * alarm_fsm.h —— 三级告警状态机（含迟滞与故障锁存）
 */
#ifndef ALARM_FSM_H
#define ALARM_FSM_H

#include <stdint.h>

typedef enum {
    ST_NORMAL = 0,
    ST_WARN,         /* 预警：> 45°C 持续 3s */
    ST_ALARM,        /* 告警：> 60°C 持续 3s */
    ST_EMERGENCY,    /* 紧急：> 75°C，立即动作 */
    ST_FAULT         /* 故障锁存：硬件故障，需人工复位 */
} alarm_state_t;

typedef struct {
    float    t_load;       /* 负载本体温度 °C */
    float    t_local;      /* 局部温度 °C */
    float    current;      /* 回路电流 A */
    float    bus_voltage;  /* 负载电压 V */
    uint16_t duty;         /* 当前输出占空比 0..10000 */
} alarm_input_t;

void          alarm_fsm_init(void);
alarm_state_t alarm_fsm_update(const alarm_input_t *in, uint32_t dt_ms);
alarm_state_t alarm_fsm_get_state(void);

/* 硬件故障（温控开关动作 / 过流 / 传感器失效）→ 锁存，禁止自动恢复加热 */
void          alarm_fsm_latch_fault(void);
uint8_t       alarm_fsm_is_latched(void);

/* 人工 / 远端复位 */
void          alarm_fsm_clear(void);

const char   *alarm_state_name(alarm_state_t s);

#endif /* ALARM_FSM_H */
