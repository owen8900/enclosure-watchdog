/**
 * app_config.h —— 项目所有可调参数集中管理
 * PC 与 MCU 共用，不依赖任何硬件头文件
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 告警阈值（°C） */
#define CFG_WARN_T              45.0f   /* 预警触发 */
#define CFG_ALARM_T             60.0f   /* 告警触发 */
#define CFG_EMERG_T             75.0f   /* 紧急触发（不去抖，立即动作） */
#define CFG_WARN_CLEAR_T        40.0f   /* 预警解除（迟滞 5°C） */
#define CFG_ALARM_CLEAR_T       55.0f   /* 告警解除（迟滞 5°C） */
#define CFG_DEBOUNCE_MS         3000u   /* 向上迁移需持续确认的时间 */

/* 故障诊断阈值 */
#define CFG_I_OPEN_A            0.05f   /* 判据1：电流低于此值视为无电流 */
#define CFG_I_OPEN_MS           2000u   /* 判据1：需持续 2 秒 */
#define CFG_I_OVER_A            2.50f   /* 判据2：过流阈值 */
#define CFG_I_MOS_SHORT_A       0.05f   /* 判据3：PWM=0 时电流大于此值 */
#define CFG_T_DRIFT_C           5.0f    /* 判据4：双探头温差阈值 */

/* 判据5：一阶热模型残差法。残差 = 实测温升速率 - ( P - k·(T - Tamb) ) / C，
 * 超限即判散热通道异常。C 与 k 的辨识方法：给恒定功率、记录温度曲线、拟合
 */
#define CFG_THERM_C_J_PER_K      9.00f   /* 负载热容（初值估算，功率级接上后辨识） J/K */
#define CFG_THERM_K_W_PER_K      0.080f  /* 散热系数（初值估算，功率级接上后辨识） W/K */
#define CFG_HEATER_PMAX_W        10.0f   /* 加热功率上限 W */
#define CFG_RESIDUAL_ABN_C_PER_S 0.50f   /* 温升速率残差阈值 °C/s */

#define CFG_R_TREND_OHM_PER_S   0.050f  /* 判据6：阻值老化斜率阈值 Ω/s */

#define CFG_HW_OVERHEAT_T       70.0f   /* KSD9700 动作点，用于故障码标注 */

/* 加热控制 */
#define CFG_HEATER_PERIOD_MS    1000u   /* 1Hz 时间比例周期 */
#define CFG_HEATER_MAX_DUTY     10000u  /* 占空比满量程（分辨率 0.01%） */
#define CFG_HEATER_TARGET       50.0f   /* 默认目标温度 */
#define CFG_HEATER_FULL_ERR     5.0f    /* 误差 ≥ 5°C 时全功率 */

/* 滤波 */
#define CFG_MAVG_N              8       /* 滑动平均窗口 */
#define CFG_R_WIN_N             30      /* 阻值趋势窗口 */
#define CFG_T_SLOPE_WIN_N       16      /* 温度趋势窗口 */

#endif /* APP_CONFIG_H */
