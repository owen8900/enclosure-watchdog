/**
 * heater.h —— 1Hz 时间比例加热控制
 */
#ifndef HEATER_H
#define HEATER_H

#include <stdint.h>

/* 硬件解耦：PWM 输出用函数指针注入，PC 测试时传假的即可 */
typedef void (*pwm_set_fn)(uint16_t duty);

void     heater_init(pwm_set_fn fn);
void     heater_set_target(float t_c);
float    heater_get_target(void);
void     heater_set_enable(uint8_t en);      /* 0 = 强制停止（安全侧动作） */
uint8_t  heater_is_enabled(void);
void     heater_tick(uint32_t dt_ms, float t_load);

uint16_t heater_get_duty(void);              /* 目标占空比 0..10000 */
uint8_t  heater_is_on(void);                 /* 当前这一瞬间输出是否导通 */

#endif /* HEATER_H */
