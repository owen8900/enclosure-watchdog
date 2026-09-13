/**
 * main_pc_test.c —— 纯逻辑模块的 PC 端单元验证
 *
 * 目的：在开发板到货之前，把告警状态机 / 故障诊断 / 滤波 / 加热控制的
 *       逻辑先在电脑上跑通，上板后只需要调硬件。
 *
 * 编译运行：见同目录 build_run.sh（在 WSL 里执行）
 */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "app_config.h"
#include "filter.h"
#include "alarm_fsm.h"
#include "diagnose.h"
#include "heater.h"

/* ================================================================
 * 测试脚手架
 * ================================================================ */
static int g_pass = 0;
static int g_fail = 0;

static void line(void)
{
    printf("----------------------------------------------------------------------\n");
}

static void head(const char *t)
{
    printf("\n");
    line();
    printf("  %s\n", t);
    line();
}

static void check(const char *what, const char *expect, const char *got)
{
    if (strcmp(expect, got) == 0) {
        printf("   [PASS] %-28s  期望=%-22s 实际=%s\n", what, expect, got);
        g_pass++;
    } else {
        printf("   [FAIL] %-28s  期望=%-22s 实际=%s\n", what, expect, got);
        g_fail++;
    }
}

/* 记录 heater 实际输出的假 PWM */
static uint16_t g_pwm_out = 0;
static void fake_pwm_set(uint16_t d) { g_pwm_out = d; }

/* 单步推进：同时跑诊断和状态机，打印状态变化 */
static void step(const diag_input_t *in, uint32_t dt,
                 alarm_state_t *pa, fault_code_t *pf, int verbose)
{
    alarm_input_t ai;
    alarm_state_t st;
    fault_code_t  fc;

    ai.t_load      = in->t_load;
    ai.t_local     = in->t_local;
    ai.current     = in->current;
    ai.bus_voltage = in->bus_voltage;
    ai.duty        = in->duty;

    fc = diagnose_run(in, dt);
    st = alarm_fsm_update(&ai, dt);

    if (fc != *pf) {
        printf("        >>> 诊断结果变化: %s\n", fault_name(fc));
        *pf = fc;
        if (fc != FAULT_NONE) {
            alarm_fsm_latch_fault();      /* 硬件故障 → 锁存 */
            printf("        >>> 已锁存故障，禁止自动恢复加热\n");
        }
    }
    if (st != *pa) {
        printf("        >>> 告警状态变化: %s\n", alarm_state_name(st));
        *pa = st;
    }
    (void)verbose;
}

/* 只跑状态机（场景 1 专测迟滞，不喂诊断模块避免互相干扰） */
static void step_fsm(const diag_input_t *in, uint32_t dt, alarm_state_t *pa)
{
    alarm_input_t ai;
    alarm_state_t st;

    ai.t_load      = in->t_load;
    ai.t_local     = in->t_local;
    ai.current     = in->current;
    ai.bus_voltage = in->bus_voltage;
    ai.duty        = in->duty;

    st = alarm_fsm_update(&ai, dt);
    if (st != *pa) {
        printf("        >>> 告警状态变化: %s\n", alarm_state_name(st));
        *pa = st;
    }
}

/* ================================================================
 * 场景 1：正常升温 → 三级告警 → 降温恢复（验证迟滞）
 * ================================================================ */
static void sc1(void)
{
    diag_input_t  in;
    alarm_state_t pa = ST_NORMAL;
    fault_code_t  pf = FAULT_NONE;
    float         T;
    int           i;

    head("场景 1：三级告警触发 + 迟滞恢复");
    printf("  设定：阈值 45/60/75°C，向上迁移需持续 3s，向下按迟滞解除\n\n");

    alarm_fsm_init();
    diagnose_init();

    in.t_env = 25.0f;
    in.current = 0.5f;
    in.bus_voltage = 5.0f;
    in.duty = CFG_HEATER_MAX_DUTY;

    printf("  --- 升温阶段（每秒 +5°C）---\n");
    for (i = 0; i < 12; i++) {
        T = 25.0f + (float)i * 5.0f;
        in.t_load  = T;
        in.t_local = T - 1.0f;
        step_fsm(&in, 1000, &pa);
        printf("        t=%2ds  负载温度=%5.1f°C   ->  %s\n",
               i, T, alarm_state_name(alarm_fsm_get_state()));
    }
    check("升温到 80°C 后的状态", "EMERGENCY(紧急)", alarm_state_name(alarm_fsm_get_state()));

    printf("\n  --- 降温阶段（每秒 -5°C）---\n");
    for (i = 0; i < 11; i++) {
        T = 80.0f - (float)i * 5.0f;
        in.t_load  = T;
        in.t_local = T - 1.0f;
        in.duty = 0;
        step_fsm(&in, 1000, &pa);
        printf("        t=%2ds  负载温度=%5.1f°C   ->  %s\n",
               i, T, alarm_state_name(alarm_fsm_get_state()));
    }
    check("降温到 30°C 后的状态", "NORMAL   (正常)", alarm_state_name(alarm_fsm_get_state()));
    (void)pf;
}

/* ================================================================
 * 场景 2：加热回路断路
 * ================================================================ */
static void sc2(void)
{
    diag_input_t  in;
    alarm_state_t pa = ST_NORMAL;
    fault_code_t  pf = FAULT_NONE;
    int           i;

    head("场景 2：加热回路断路（PWM 有输出但电流为 0）");
    printf("  判据：duty>0 且 I<0.05A，需【持续 2 秒】\n\n");

    alarm_fsm_init();
    diagnose_init();

    in.t_env = 25.0f; in.t_load = 30.0f; in.t_local = 30.0f;
    in.duty = 5000;                     /* 50% 占空比 */
    in.current = 0.0f;                  /* 但电流为 0 —— 断路 */
    in.bus_voltage = 5.0f;

    for (i = 0; i < 6; i++) {
        step(&in, 500, &pa, &pf, 0);
        printf("        已持续 %4d ms\n", (i + 1) * 500);
    }
    check("断路诊断", "加热回路断路", fault_name(pf));
    check("告警状态", "FAULT    (故障锁存)", alarm_state_name(alarm_fsm_get_state()));
}

/* ================================================================
 * 场景 3：过流 / 短路
 * ================================================================ */
static void sc3(void)
{
    diag_input_t  in;
    alarm_state_t pa = ST_NORMAL;
    fault_code_t  pf = FAULT_NONE;

    head("场景 3：过流 / 短路（应立即切断）");
    printf("  判据：I > %.2f A\n\n", CFG_I_OVER_A);

    alarm_fsm_init();
    diagnose_init();

    in.t_env = 25.0f; in.t_load = 30.0f; in.t_local = 30.0f;
    in.duty = 10000; in.current = 3.20f; in.bus_voltage = 5.0f;

    step(&in, 100, &pa, &pf, 0);
    printf("        电流 = 3.20 A\n");
    check("过流诊断", "过流 / 短路", fault_name(pf));
    check("告警状态", "FAULT    (故障锁存)", alarm_state_name(alarm_fsm_get_state()));
}

/* ================================================================
 * 场景 4：MOS 击穿直通
 * ================================================================ */
static void sc4(void)
{
    diag_input_t  in;
    alarm_state_t pa = ST_NORMAL;
    fault_code_t  pf = FAULT_NONE;

    head("场景 4：MOS 击穿直通（已关断但回路仍有电流）");
    printf("  判据：duty == 0 但 I > %.2f A\n\n", CFG_I_MOS_SHORT_A);

    alarm_fsm_init();
    diagnose_init();

    in.t_env = 25.0f; in.t_load = 30.0f; in.t_local = 30.0f;
    in.duty = 0; in.current = 0.50f; in.bus_voltage = 5.0f;

    step(&in, 100, &pa, &pf, 0);
    printf("        命令占空比 = 0，实测电流 = 0.50 A\n");
    check("MOS 击穿诊断", "MOS 击穿直通", fault_name(pf));
}

/* ================================================================
 * 场景 5：温度传感器漂移
 * ================================================================ */
static void sc5(void)
{
    diag_input_t  in;
    alarm_state_t pa = ST_NORMAL;
    fault_code_t  pf = FAULT_NONE;

    head("场景 5：温度传感器漂移（双探头偏差超限）");
    printf("  判据：两探头均 >45°C 但偏差 > %.1f°C\n\n", CFG_T_DRIFT_C);

    alarm_fsm_init();
    diagnose_init();

    in.t_env = 25.0f;
    in.t_load  = 70.0f;      /* 负载探头 */
    in.t_local = 59.0f;      /* 局部探头，差 11°C */
    in.duty = 0; in.current = 0.0f; in.bus_voltage = 0.0f;

    step(&in, 100, &pa, &pf, 0);
    printf("        负载探头 = 70.0°C   局部探头 = 59.0°C   偏差 = 11.0°C\n");
    check("传感器漂移诊断", "温度传感器漂移", fault_name(pf));
}

/* ================================================================
 * 场景 6：散热异常（一阶热模型残差法）
 * ================================================================ */
static void sc6(void)
{
    diag_input_t  in;
    alarm_state_t pa = ST_NORMAL;
    fault_code_t  pf = FAULT_NONE;
    float         T = 30.0f;
    float         p_w;
    float         expected;
    int           i;

    head("场景 6：散热异常（电流正常但升温比模型预测快）");
    printf("  方法：一阶热模型残差  残差 = 实测 dT/dt - 预期 dT/dt\n");
    printf("        预期 = (Pmax×占空比 - k·(T-Tamb)) / C\n");
    printf("        C=%.1f J/K, k=%.3f W/K, Pmax=%.0f W, 残差阈值=%.2f °C/s\n\n",
           CFG_THERM_C_J_PER_K, CFG_THERM_K_W_PER_K,
           CFG_HEATER_PMAX_W, CFG_RESIDUAL_ABN_C_PER_S);

    alarm_fsm_init();
    diagnose_init();

    in.t_env = 25.0f; in.duty = 8000; in.current = 0.50f; in.bus_voltage = 5.0f;

    p_w      = CFG_HEATER_PMAX_W * 0.80f;
    expected = (p_w - CFG_THERM_K_W_PER_K * (T - in.t_env)) / CFG_THERM_C_J_PER_K;
    printf("        起始点 T=%.0f°C 时，模型预期温升速率 = %.3f °C/s\n", T, expected);
    printf("        而实测温升速率为 2.000 °C/s（人为制造散热异常）\n\n");

    for (i = 0; i < 60; i++) {
        T += 0.20f;                     /* 每 100ms 升 0.2°C  →  2.0 °C/s */
        in.t_load  = T;
        in.t_local = T - 1.0f;
        step(&in, 100, &pa, &pf, 0);
    }
    printf("        实测温升速率 = %.3f °C/s\n", diagnose_get_temp_slope());
    check("散热异常诊断", "散热异常", fault_name(pf));
}

/* ================================================================
 * 场景 7：元件老化趋势预警（R = U/I 持续上升）
 * ================================================================ */
static void sc7(void)
{
    diag_input_t  in;
    alarm_state_t pa = ST_NORMAL;
    fault_code_t  pf = FAULT_NONE;
    int           i;

    head("场景 7：元件老化趋势预警（R = U/I 持续上升）");
    printf("  判据：阻值斜率 > %.3f Ω/s\n\n", CFG_R_TREND_OHM_PER_S);

    alarm_fsm_init();
    diagnose_init();

    in.t_env = 25.0f; in.t_load = 30.0f; in.t_local = 30.0f;
    in.duty = 5000; in.current = 0.50f;   /* 电流恒定 */

    for (i = 0; i < 40; i++) {
        in.bus_voltage = 5.00f + (float)i * 0.010f;   /* 电压缓慢上升 → R 上升 */
        step(&in, 100, &pa, &pf, 0);
    }
    printf("        起始 R = %.3f Ω  →  当前 R = %.3f Ω\n",
           5.00f / 0.50f, diagnose_get_load_r());
    printf("        阻值斜率 = %.4f Ω/s\n", diagnose_get_r_slope());
    check("老化趋势诊断", "元件老化趋势预警", fault_name(pf));
}

/* ================================================================
 * 场景 8：加热闭环控制（一阶热模型）
 * ================================================================ */
static void sc8(void)
{
    float T = 25.0f;
    float Tamb = 25.0f;
    const float C = 9.0f;       /* 热容 J/K（与 app_config 里的辨识值一致） */
    const float K = 0.080f;     /* 散热系数 W/K */
    const float Pmax = 10.0f;   /* 最大加热功率 W */
    int   i;
    int   n;

    head("场景 8：加热闭环控制（一阶热模型仿真，200 秒）");
    printf("  模型：C·dT/dt = P - k·(T - Tamb)\n");
    printf("        C=%.0f J/K, k=%.3f W/K, Pmax=%.0f W, 目标=%.0f°C\n",
           C, K, Pmax, CFG_HEATER_TARGET);
    printf("        时间常数 τ = C/k = %.0f s（真实水泥电阻就是这个量级）\n\n",
           C / K);

    heater_init(fake_pwm_set);
    heater_set_target(CFG_HEATER_TARGET);
    heater_set_enable(1);

    printf("   时间(s)   温度(°C)   占空比(%%)   导通\n");
    printf("   --------  ---------  ----------  -----\n");

    for (i = 0; i < 20000; i++) {                /* 20000 × 10ms = 200s */
        float duty_ratio = (float)heater_get_duty() / (float)CFG_HEATER_MAX_DUTY;
        float P = Pmax * duty_ratio;

        heater_tick(10, T);
        T += (P - K * (T - Tamb)) / C * 0.010f;

        if ((i % 2000) == 0) {                   /* 每 20s 打印一次 */
            n = i / 2000;
            printf("   %6d    %7.2f    %8.2f    %s\n",
                   n * 20, T,
                   (float)heater_get_duty() / 100.0f,
                   heater_is_on() ? "开" : "关");
        }
    }

    printf("\n  最终温度 = %.2f°C（目标 %.0f°C，误差 %+.2f°C）\n",
           T, CFG_HEATER_TARGET, T - CFG_HEATER_TARGET);
    if (fabsf(T - CFG_HEATER_TARGET) < 3.0f) {
        printf("   [PASS] 闭环控制收敛到目标温度 ±3°C 以内\n");
        g_pass++;
    } else {
        printf("   [FAIL] 未收敛\n");
        g_fail++;
    }
}

/* ================================================================
 * 主程序
 * ================================================================ */
int main(void)
{
    printf("\n");
    printf("======================================================================\n");
    printf("  模拟控制柜健康监测与故障诊断系统 —— 核心算法 PC 端验证\n");
    printf("  模块：告警状态机 / 多源融合诊断 / 滤波 / 加热控制\n");
    printf("  注意：本程序不依赖任何硬件，纯逻辑验证\n");
    printf("======================================================================\n");

    sc1();
    sc2();
    sc3();
    sc4();
    sc5();
    sc6();
    sc7();
    sc8();

    printf("\n");
    printf("======================================================================\n");
    printf("  测试汇总：PASS = %d   FAIL = %d\n", g_pass, g_fail);
    printf("======================================================================\n\n");

    return (g_fail == 0) ? 0 : 1;
}
