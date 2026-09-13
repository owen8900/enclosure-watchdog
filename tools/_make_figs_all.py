# -*- coding: utf-8 -*-
"""生成项目步骤图与流程图（PNG）—— 最终版"""
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'SimSun']
plt.rcParams['axes.unicode_minus'] = False

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'images')
os.makedirs(OUT, exist_ok=True)

C = {
    'blue':   ('#DCE9F7', '#2E6DB4'),
    'green':  ('#DDF2E3', '#2E8B57'),
    'orange': ('#FDEBD3', '#D2691E'),
    'red':    ('#FADBD8', '#C0392B'),
    'purple': ('#E8DEF7', '#6A4C93'),
    'gray':   ('#ECEFF1', '#546E7A'),
    'yellow': ('#FCF3CF', '#B7950B'),
    'teal':   ('#D5F0F0', '#00838F'),
}


def newfig(title, w=13, h=8.5):
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, 100); ax.set_ylim(0, 100); ax.axis('off')
    ax.set_title(title, fontsize=17, fontweight='bold', pad=14, color='#1A237E')
    return fig, ax


def box(ax, x, y, w, h, text, color='blue', fs=10.5, tc='#111111', lw=1.6,
        align='center', rs=1.2, z=2):
    fc, ec = C[color]
    if align == 'left':
        ax.add_patch(FancyBboxPatch((x, y - h / 2), w, h,
                     boxstyle=f"round,pad=0.0,rounding_size={rs}",
                     linewidth=lw, facecolor=fc, edgecolor=ec, zorder=z))
        ax.text(x + 2.4, y, text, ha='left', va='center', fontsize=fs,
                color=tc, zorder=z + 1, linespacing=1.55)
    else:
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                     boxstyle=f"round,pad=0.0,rounding_size={rs}",
                     linewidth=lw, facecolor=fc, edgecolor=ec, zorder=z))
        ax.text(x, y, text, ha='center', va='center', fontsize=fs,
                color=tc, zorder=z + 1, linespacing=1.55)


def arrow(ax, *pts, color='#455A64', lw=1.8, ls='-', style='-|>', ms=15,
          text=None, tfs=8.5, tpos=0.5, tcolor=None):
    for i in range(len(pts) - 1):
        last = (i == len(pts) - 2)
        (x1, y1), (x2, y2) = pts[i], pts[i + 1]
        ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2),
                     arrowstyle=(style if last else '-'),
                     mutation_scale=ms, linewidth=lw, color=color,
                     linestyle=ls, zorder=1, shrinkA=0, shrinkB=0))
    if text:
        (x1, y1), (x2, y2) = pts[0], pts[-1]
        mx, my = x1 + (x2 - x1) * tpos, y1 + (y2 - y1) * tpos
        ax.text(mx, my, text, fontsize=tfs, ha='center', va='center',
                color=tcolor or color, zorder=4,
                bbox=dict(fc='white', ec='none', pad=1.2, alpha=0.92))


def save(fig, name):
    fig.savefig(os.path.join(OUT, name), dpi=200, bbox_inches='tight',
                facecolor='white')
    plt.close(fig)
    print('OK', name)


def band(ax, y0, y1, label, color):
    ax.add_patch(FancyBboxPatch((2, y0), 96, y1 - y0,
                 boxstyle="round,pad=0.0,rounding_size=0.8",
                 linewidth=0, facecolor=color, zorder=0))
    ax.text(6, (y0 + y1) / 2, label, ha='center', va='center',
            fontsize=12, fontweight='bold', color='#37474F', zorder=1,
            linespacing=1.4)


# ============================================================
# 图1  端-边-云 数据流图
# ============================================================
def fig1():
    fig, ax = newfig('图1  系统总体架构与数据流（端 - 边 - 云）', 13, 9)
    band(ax, 76, 95, '感\n知\n层', '#E3F2FD')
    band(ax, 44, 73, '边\n缘\n控\n制\n层', '#E8F5E9')
    band(ax, 24, 41, '通\n信\n层', '#FFF3E0')
    band(ax, 3, 21, '应\n用\n层', '#F3E5F5')

    y = 85.5
    box(ax, 25, y, 20, 12, 'SHT30 #1\n环境温度', 'blue', 10)
    box(ax, 49, y, 22, 12, 'DS18B20\n负载本体温度', 'blue', 10)
    box(ax, 73, y, 20, 12, 'SHT30 #2\n局部温度', 'blue', 10)
    box(ax, 93, y, 12, 12, 'INA219\n电流/电压', 'blue', 9)

    y2 = 58.5
    box(ax, 17, y2, 22, 15, 'MOS + PWM\n加热执行', 'green', 10)
    box(ax, 41, y2, 18, 15, '声光告警\n蜂鸣器/RGB', 'green', 9.8)
    box(ax, 63, y2, 26, 18, 'STM32H750VB\nFreeRTOS 8 任务\n告警状态机 / 故障诊断', 'green', 10.5)
    box(ax, 86, y2, 18, 15, '2.8" TFT\n本地 HMI', 'green', 10)

    y3 = 32.5
    box(ax, 33, y3, 26, 12, 'W5500\n以太网（主链路）', 'orange', 10.5)
    box(ax, 68, y3, 26, 12, 'ESP-01S\nWiFi（备用链路）', 'orange', 10.5)

    y4 = 12
    box(ax, 21, y4, 28, 13, 'SCADA / PLC / 组态\n（Modbus TCP 从站 502）', 'purple', 9.5)
    box(ax, 51, y4, 20, 13, '华为云 IoTDA\n（MQTT）', 'purple', 9.5)
    box(ax, 80, y4, 30, 13, '可视化大屏 + 趋势分析\n（ECharts / 数据分析）', 'purple', 9.5)

    for x in (25, 49, 73, 93):
        arrow(ax, (x, 79.5), (x, 66.8), color='#1565C0')
    ax.text(50, 81.5, 'I2C / 单总线  ·  100ms 周期采样', fontsize=9, ha='center',
            color='#1565C0', bbox=dict(fc='white', ec='none', pad=1.2))

    arrow(ax, (41, 51), (36, 38.7), color='#00838F', text='SPI', tfs=8.5)
    arrow(ax, (63, 51), (66, 38.7), color='#00838F', text='UART', tfs=8.5)

    arrow(ax, (28, 26.5), (22, 18.8), color='#6A1B9A')
    arrow(ax, (63, 26.5), (52, 18.8), color='#6A1B9A')
    arrow(ax, (76, 26.5), (80, 18.8), color='#6A1B9A')

    ax.text(50, 0.5, '数据闭环：传感器 → 边缘诊断 → 本地告警/显示 → 上云 → 大屏分析',
            fontsize=10, ha='center', color='#37474F', style='italic')
    save(fig, 'fig1_architecture.png')


# ============================================================
# 图2  硬件结构与总线分配
# ============================================================
def fig2():
    fig, ax = newfig('图2  硬件结构图与总线分配', 13, 9)

    box(ax, 17, 100, 0.1, 0.1, '', 'gray')
    box(ax, 50, 50, 30, 17, 'STM32H750VB\n480MHz Cortex-M7\n1MB RAM · 16MB QSPI Flash',
        'green', 11.5)

    box(ax, 17, 81, 26, 11, 'SHT30 #1  环境温度\nI2C1', 'blue', 9.5)
    box(ax, 50, 81, 27, 11, 'DS18B20  负载本体温度\n单总线 + 4.7kΩ 上拉', 'blue', 9.5)
    box(ax, 83, 81, 26, 11, 'SHT30 #2  局部温度\nI2C2', 'blue', 9.5)

    box(ax, 12, 55, 19, 13, 'ESP-01S\nWiFi\nUART2', 'orange', 9.5)
    box(ax, 90, 55, 19, 13, 'INA219\n电流 / 电压\nI2C1 @0x40', 'blue', 9.5)

    box(ax, 14, 26, 22, 13, '2.8" TFT\nILI9341\nSPI1 + 触摸', 'gray', 9.5)
    box(ax, 50, 26, 24, 12, 'W5500 以太网\nSPI1', 'orange', 9.5)
    box(ax, 86, 26, 22, 13, '板载 TF 卡座\nSDMMC / SPI', 'gray', 9.5)

    for (x1, y1, x2, y2, t, cx, cy) in [
        (17, 75.5, 38, 58.5, 'I2C1', 24, 68),
        (50, 75.5, 50, 58.5, '单总线', 55.5, 68),
        (83, 75.5, 62, 58.5, 'I2C2', 76, 68),
        (90, 48.5, 65, 50, 'I2C1', 79, 47),
        (12, 48.5, 36, 50, 'UART2', 21, 47),
        (14, 32.5, 35, 45, 'SPI1', 23, 39),
        (50, 32.5, 50, 41.5, 'SPI1', 54.5, 37),
        (86, 32.5, 65, 45, 'SDMMC', 79, 39),
    ]:
        arrow(ax, (x1, y1), (x2, y2), color='#546E7A', lw=1.5)
        ax.text(cx, cy, t, fontsize=8.5, ha='center', va='center', color='#37474F',
                bbox=dict(fc='white', ec='none', pad=1.0))

    box(ax, 50, 8, 90, 11,
        'SPI1 为共享总线：TFT / W5500 分片选，用互斥量保护（取锁带 50ms 超时）\n'
        '调试口必须选 SWD（Serial Wire），不能选 JTAG 5-pin',
        'yellow', 10)

    ax.text(50, 94.5, '禁占引脚：PB2 / PB6 / PD11~PD13 / PE2（板载 QSPI Flash 占用，占了程序下不进去）',
            fontsize=10.5, ha='center', color='#C0392B', fontweight='bold',
            bbox=dict(fc='#FDEDEC', ec='#C0392B', boxstyle='round,pad=0.5,rounding_size=0.6'))
    save(fig, 'fig2_hardware.png')


# ============================================================
# 图3  功率级接线与三重安全链
# ============================================================
def fig3():
    fig, ax = newfig('图3  加热功率级接线与三重安全链', 13.5, 8)

    y = 76
    box(ax, 9, y, 13, 11, '5V+\n加热电源', 'red', 10)
    box(ax, 27, y, 17, 11, 'PPTC 1.1A\n自恢复保险丝', 'yellow', 9.5)
    box(ax, 47, y, 17, 11, 'KSD9700\n70°C 常闭', 'yellow', 9.5)
    box(ax, 68, y, 18, 11, 'INA219\n板载 0.1Ω 分流', 'blue', 9.5)
    box(ax, 89, y, 17, 11, '水泥电阻\n10Ω / 10W', 'red', 9.5)

    for x1, x2 in ((15.5, 18.5), (35.5, 38.5), (55.5, 59), (77, 80.5)):
        arrow(ax, (x1, y), (x2, y), color='#C0392B', lw=2.2)

    box(ax, 40, 62, 40, 12, '导热铝片 40×40×2.3mm（自带导热双面胶）\nDS18B20 + KSD9700 贴其上；SHT30#2 放旁边 10-20mm',
        'orange', 9.5)

    arrow(ax, (89, 70.5), (89, 47), color='#C0392B', lw=2.2)
    box(ax, 89, 41, 17, 12, 'IRLZ44N\n低边开关\nG / D / S', 'green', 9.5)
    arrow(ax, (89, 35), (89, 24), color='#C0392B', lw=2.2)
    box(ax, 89, 18, 17, 10, 'GND ★\n星型接地点', 'gray', 10)

    box(ax, 36, 30, 42, 15,
        'PA8 (TIM1_CH1) ──[100Ω]──┬── G\n'
        '                          [10kΩ] 下拉\n'
        '                          GND\n'
        '1Hz 时间比例控制（占空比 30% = 导通 300ms）', 'blue', 9.5)

    ax.text(50, 97, '三重安全冗余：软件看门狗  +  硬件温控开关  +  PPTC 保险丝',
            fontsize=12.5, ha='center', fontweight='bold', color='#C0392B',
            bbox=dict(fc='#FDEDEC', ec='#C0392B', boxstyle='round,pad=0.5,rounding_size=0.6'))
    ax.text(50, 89, '故障时一律 fail-safe 切断加热（宁可误停，不可炸机）',
            fontsize=10.5, ha='center', color='#37474F')

    ax.text(50, 8, '全链路免焊：弹簧接线端子夹住引线 + INA219 螺钉端子 + MOS 插面包板',
            fontsize=10.5, ha='center', color='#2E8B57', fontweight='bold')
    ax.text(50, 2.5, '实测电流 0.5A（单通道）· 整机预算 1.13A（5V/2.5A 电源余量充足）',
            fontsize=9.5, ha='center', color='#546E7A')
    save(fig, 'fig3_power.png')


# ============================================================
# 图4  项目实施步骤流程 P0-P7
# ============================================================
def fig4():
    fig, ax = newfig('图4  项目实施步骤流程（P0 → P7）', 13, 11)

    stages = [
        ('P0', '硬件自检与工具链打通', '2026.09下–10上', '串口打印 / RGBLED / TFT 三个里程碑；引脚表定稿', 'blue'),
        ('P1', '外设逐个点亮', '2026.10（3周）', 'SHT30 / INA219 / DS18B20 / TF卡 / W5500 五个 demo', 'blue'),
        ('暂停', '软考中级（项目挂起）', '2026.11', '—', 'gray'),
        ('P2', 'RTOS 任务架构', '2026.12（3周）', '8 任务 + 队列 + SPI 互斥量；采集 1h 零复位', 'green'),
        ('P3', '告警与故障诊断', '2027.01 寒假（3周）', '告警状态机 + 迟滞 + 趋势预警 + 故障注入全过', 'green'),
        ('P4', '通信与上云', '2027.02 寒假（3周）', 'Modbus TCP 从站 + MQTT + 断网续传', 'orange'),
        ('P5', '结构装配与安全链', '2027.02（1周）', '亚克力装配 + 温控开关 + 保险丝 + 线束整理', 'orange'),
        ('P6', '大屏与参赛材料', '2027.03（4周）', '可视化大屏 + 多套申报书 + 演示视频', 'purple'),
        ('P7', '参赛', '2027.04 – 07', '蓝桥杯(4月) / 报名 / 省赛答辩 / 嵌入式设计师(5月)', 'red'),
    ]
    top, bot = 88, 8
    step = (top - bot) / (len(stages) - 1)
    for i, (tag, name, t, out, col) in enumerate(stages):
        y = top - i * step
        box(ax, 9, y, 13, 7.4, tag, col, 11)
        box(ax, 51.5, y, 69, 7.4, f'{name}    【{t}】\n{out}', col, 9.5, align='left')
        if i < len(stages) - 1:
            arrow(ax, (51.5, y - 3.7), (51.5, y - step + 3.7), color='#78909C', lw=1.6)

    ax.text(50, 96, '★ P0 是最关键的一关：过不了工具链，买再多传感器也是摆设',
            fontsize=11.5, ha='center', fontweight='bold', color='#C0392B')
    ax.text(50, 2, '★ 2027 赛季主攻 2 个：全国大学生物联网设计竞赛（华为杯）+ 西门子杯；其余复用材料',
            fontsize=10, ha='center', color='#37474F', style='italic')
    save(fig, 'fig4_steps.png')


# ============================================================
# 图5  固件任务架构
# ============================================================
def fig5():
    fig, ax = newfig('图5  固件软件架构（FreeRTOS 8 任务）', 13.5, 9)

    y1 = 84
    box(ax, 20, y1, 27, 13, 'heater_ctrl\n优先级 8 · 10ms\n1Hz 时间比例控制', 'red', 9.5)
    box(ax, 50, y1, 27, 13, 'alarm_fsm\n优先级 7 · 100ms\n告警状态机 + 迟滞', 'red', 9.5)
    box(ax, 80, y1, 27, 13, 'sensor\n优先级 6 · 100ms\nI2C / 单总线采集', 'orange', 9.5)

    box(ax, 50, 58, 96, 13, 'FreeRTOS 内核   抢占式调度 · 1ms tick · heap_4（初始化后不再 malloc）',
        'green', 11.5)

    y3 = 33
    box(ax, 11, y3, 19, 13, 'diagnose\nP5 · 1s\n多源融合诊断\nR=U/I 老化趋势', 'purple', 8.8)
    box(ax, 31, y3, 19, 13, 'net_w5500\nP4 · 事件\nModbus TCP\n+ MQTT', 'blue', 8.8)
    box(ax, 51, y3, 19, 13, 'ui_lvgl\nP3 · 200ms\nTFT 局部刷新', 'blue', 8.8)
    box(ax, 71, y3, 19, 13, 'storage\nP2 · 1s\nFatFS 环形日志\n断网续传', 'blue', 8.8)
    box(ax, 91, y3, 17, 13, 'monitor\nP1 · 1s\n栈水位/喂狗\n健康自检', 'gray', 8.8)

    for x in (20, 50, 80):
        arrow(ax, (x, 77.5), (x, 64.7), color='#546E7A', lw=1.6)
    for x in (11, 31, 51, 71, 91):
        arrow(ax, (x, 51.2), (x, 39.8), color='#546E7A', lw=1.6)

    box(ax, 50, 12, 94, 13,
        '通信机制：队列 xQueue（数据） · 互斥量（SPI1 共享总线，带 50ms 超时） · 任务通知（ISR 唤醒）',
        'yellow', 10.5)

    ax.text(50, 95, '规则：ISR 只做 xTaskNotifyFromISR；绝不在 ISR 中碰 SPI/I2C；绝不调用阻塞 API',
            fontsize=10.5, ha='center', color='#C0392B', fontweight='bold')
    ax.text(50, 2.5, '栈大小由 uxTaskGetStackHighWaterMark() 实测调优，保留 ≥30% 余量',
            fontsize=9.5, ha='center', color='#546E7A', style='italic')
    save(fig, 'fig5_tasks.png')


# ============================================================
# 图6  告警状态机
# ============================================================
def fig6():
    fig, ax = newfig('图6  三级告警状态机（含迟滞与故障锁存）', 13.5, 8.5)

    box(ax, 17, 58, 26, 15, 'NORMAL 正常\n绿 LED 常亮\n持续采样与记录', 'green', 10.5)
    box(ax, 50, 84, 26, 15, 'WARN 预警\n蓝 LED 慢闪 0.5Hz\n> 45°C 持续 3s', 'blue', 10)
    box(ax, 83, 58, 26, 15, 'ALARM 告警\n红 LED + 蜂鸣 2kHz 断续\n> 60°C 持续 3s', 'orange', 9.8)
    box(ax, 83, 20, 26, 15, 'EMERGENCY 紧急\n切断加热 + 蜂鸣连续\n> 75°C', 'red', 10)
    box(ax, 30, 20, 30, 15, 'FAULT 故障锁存\n锁定，禁止自动恢复加热\n需人工 / 远端复位', 'gray', 10)

    arrow(ax, (28, 64.5), (40, 78), color='#2E8B57', lw=2)
    ax.text(30, 74.5, '温升', fontsize=9, color='#2E8B57')

    arrow(ax, (62, 78), (74, 64.5), color='#D2691E', lw=2)
    ax.text(72, 74.5, '温升', fontsize=9, color='#D2691E')

    arrow(ax, (83, 50.5), (83, 27.7), color='#C0392B', lw=2)
    ax.text(89, 39, '温升', fontsize=9, color='#C0392B')

    arrow(ax, (70, 20), (45, 20), color='#C0392B', lw=2)
    ax.text(57, 25, '任一硬件故障', fontsize=9.5, color='#C0392B', ha='center')
    ax.text(57, 15.5, 'KSD9700 动作 / 过流 / 传感器失效', fontsize=8.2,
            ha='center', color='#C0392B')

    arrow(ax, (40, 78), (28, 64.5), color='#546E7A', lw=1.6, ls='--')
    ax.text(27, 70.5, '< 40°C\n迟滞解除', fontsize=8.5, ha='center', color='#546E7A')

    arrow(ax, (74, 64.5), (62, 78), color='#546E7A', lw=1.6, ls='--')
    ax.text(75, 70.5, '< 55°C\n迟滞解除', fontsize=8.5, ha='center', color='#546E7A')

    arrow(ax, (30, 27.7), (17, 50.5), color='#37474F', lw=1.6, ls='--')
    ax.text(13, 39, '人工复位', fontsize=9, ha='center', color='#37474F')

    ax.text(50, 96.5, '迟滞带：触发 45 / 60 / 75°C，解除 40 / 55°C —— 防止阈值附近反复抖动',
            fontsize=11, ha='center', fontweight='bold', color='#B7950B')
    ax.text(50, 5, '★ 硬件故障必须【锁存】：KSD9700 自动复位，若不锁存会出现"加热-断开-再加热"的打嗝循环',
            fontsize=9.8, ha='center', color='#C0392B', style='italic')
    save(fig, 'fig6_statemachine.png')


# ============================================================
# 图7  多源融合故障诊断流程
# ============================================================
def fig7():
    fig, ax = newfig('图7  多源融合故障诊断判据流程', 14, 10)

    box(ax, 40, 94, 58, 8, '每 1s 采集：T_env · T_load · T_local · I · V_bus · duty',
        'gray', 10.5)

    nodes = [
        ('判据 1   duty > 0 且 I < 0.05A 持续 2s ?', '加热回路断路\n（加热丝断 / 端子松 /\n保险丝熔断 / 温控开关动作）', 'red'),
        ('判据 2   I > 2.5A ?', '过流 / 短路\n→ 立即切断 MOS', 'red'),
        ('判据 3   duty = 0 但 I > 0.05A ?', 'MOS 击穿直通\n→ 危险，触发紧急', 'red'),
        ('判据 4   | T_load - T_local | > 5°C ?', '温度传感器漂移\n（非真实过热）', 'orange'),
        ('判据 5   I 正常 且 dT/dt > 阈值 ?', '散热异常 / 负载异常', 'orange'),
        ('判据 6   R = V / I 滑动窗口斜率 > 阈值 ?', '元件老化趋势预警\n（健康度退化）', 'yellow'),
    ]
    y0, dy = 84, 12.5
    for i, (cond, result, col) in enumerate(nodes):
        y = y0 - i * dy
        box(ax, 40, y, 56, 8.2, cond, 'blue', 10)
        box(ax, 87, y, 24, 8.2, result, col, 8.2)
        arrow(ax, (68, y), (74.5, y), color='#C0392B', lw=1.8)
        ax.text(71, y + 3.3, '是', fontsize=9, ha='center', color='#C0392B',
                bbox=dict(fc='white', ec='none', pad=0.8))
        if i < len(nodes) - 1:
            arrow(ax, (40, y - 4.1), (40, y - dy + 4.1), color='#546E7A', lw=1.8)
            ax.text(35, y - dy / 2, '否', fontsize=9, ha='center', color='#546E7A')

    ylast = y0 - len(nodes) * dy
    box(ax, 40, ylast, 56, 8.2, '正常：记录数据 → 上报云端 → 刷新大屏', 'green', 10.5)
    arrow(ax, (40, nodes and y0 - (len(nodes) - 1) * dy - 4.1), (40, ylast + 4.1),
          color='#546E7A', lw=1.8)
    ax.text(35, (ylast + y0 - (len(nodes) - 1) * dy) / 2 - 2.5, '否', fontsize=9,
            ha='center', color='#546E7A')

    arrow(ax, (40, 90), (40, 88.4), color='#546E7A', lw=1.8)

    ax.text(50, 1.5, '技术内核：把"温度超限就报警"升级为"多源交叉判据的故障诊断 + 健康度趋势预测"',
            fontsize=10.5, ha='center', color='#1A237E', fontweight='bold')
    save(fig, 'fig7_diagnosis.png')


# ============================================================
# 图8  硬件制作步骤图
# ============================================================
def fig8():
    fig, ax = newfig('图8  硬件制作步骤（12 步）', 13.5, 10)

    steps = [
        '下载正点原子资料包 A 盘，\n核对《MiniPro H750 引脚分配表》',
        '装工具链：Keil MDK + QSPI\nFlash 下载算法（勿用 OpenOCD）',
        'M1 串口打印（撞 128KB Flash 坑，\n须烧到外部 0x90000000）',
        'M2 板载 RGBLED 三色闪烁\nPB4红 / PE5绿 / PE6蓝',
        'M3 TFT 显示（通用 SPI 屏\n需自备 ILI9341 驱动）',
        '面包板搭控制部分，I2C 扫描\n确认 0x40(INA219) / 0x44(SHT30)',
        '万用表通断档：确认 5V/GND\n无短路；KSD9700 常温导通',
        '弹簧端子串接功率回路：PPTC →\nKSD9700 → INA219 → 电阻 → MOS',
        'MOS 栅极接 100Ω 串阻\n+ 10kΩ 下拉，全插面包板',
        '先上控制电、再上加热电，\n万用表量电流 ≈ 0.5A',
        '贴传感器到导热铝片，装盒\n+ 扎带理线（透明盒要好看）',
        '72h 连续跑测 + 故障注入\n（拔传感器/拔网线/断丝/短接）',
    ]
    ys = [86, 71, 56, 41, 26, 11]
    for i, s in enumerate(steps):
        col = i // 6
        row = i % 6
        x0 = 4 if col == 0 else 54
        y = ys[row]
        box(ax, x0, y, 42, 12, '', 'blue' if i < 6 else 'green', 9)
        ax.text(x0 + 5, y, str(i + 1), fontsize=16, ha='center', va='center',
                color='#1A237E', fontweight='bold', zorder=5)
        ax.text(x0 + 10, y, s, fontsize=8.8, ha='left', va='center',
                color='#111111', zorder=5, linespacing=1.6)
        if row < 5:
            arrow(ax, (x0 + 21, y - 6.1), (x0 + 21, y - 15 + 6.1), color='#78909C', lw=1.6)

    arrow(ax, (46, 11), (50, 11), (50, 86), (54, 86), color='#78909C', lw=1.8)

    ax.text(50, 96, '★ 步骤 1~5 不需要任何新元件；步骤 7 之后才碰功率级',
            fontsize=11.5, ha='center', fontweight='bold', color='#2E8B57')
    save(fig, 'fig8_build.png')


for f in (fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8):
    f()
print('=== all figures done ===')
