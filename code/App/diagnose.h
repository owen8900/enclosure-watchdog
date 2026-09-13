/**
 * diagnose.h —— 多源融合故障诊断（项目技术内核）
 */
#ifndef DIAGNOSE_H
#define DIAGNOSE_H

#include <stdint.h>

typedef enum {
    FAULT_NONE = 0,
    FAULT_OPEN_CIRCUIT,      /* 加热回路断路：duty>0 但 I≈0 持续 2s */
    FAULT_OVERCURRENT,       /* 过流/短路：I > 2.5A */
    FAULT_MOS_SHORT,         /* MOS 击穿直通：duty=0 但 I≠0 */
    FAULT_SENSOR_DRIFT,      /* 温度传感器漂移：双探头偏差超限 */
    FAULT_THERMAL_ABNORMAL,  /* 散热异常：I 正常但温升速率超限 */
    FAULT_AGING_WARNING      /* 元件老化趋势预警：R=U/I 持续上升 */
} fault_code_t;

typedef struct {
    float    t_env;          /* 环境温度 °C */
    float    t_load;         /* 负载本体温度 °C（DS18B20） */
    float    t_local;        /* 局部温度 °C（SHT30#2） */
    float    current;        /* 回路电流 A（INA219） */
    float    bus_voltage;    /* 负载电压 V（INA219 电压寄存器） */
    uint16_t duty;           /* 当前输出占空比 0..10000 */
} diag_input_t;

void          diagnose_init(void);
fault_code_t  diagnose_run(const diag_input_t *in, uint32_t dt_ms);

float         diagnose_get_load_r(void);      /* 最近一次算出的 R = U/I */
float         diagnose_get_r_slope(void);     /* 阻值斜率 Ω/s */
float         diagnose_get_temp_slope(void);  /* 温升速率 °C/s */

const char   *fault_name(fault_code_t f);

#endif /* DIAGNOSE_H */
