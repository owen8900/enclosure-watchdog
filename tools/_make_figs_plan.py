# -*- coding: utf-8 -*-
"""策划书专用插图：甘特图 / 软件开发流程 / 硬件制作流程 / 联调流程"""
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle, Polygon

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


def arrow(ax, *pts, color='#455A64', lw=1.8, ls='-', style='-|>', ms=15):
    for i in range(len(pts) - 1):
        last = (i == len(pts) - 2)
        ax.add_patch(FancyArrowPatch(pts[i], pts[i + 1],
                     arrowstyle=(style if last else '-'),
                     mutation_scale=ms, linewidth=lw, color=color,
                     linestyle=ls, zorder=1, shrinkA=0, shrinkB=0))


def save(fig, name):
    fig.savefig(os.path.join(OUT, name), dpi=200, bbox_inches='tight',
                facecolor='white')
    plt.close(fig)
    print('OK', name)


# ============================================================
# 图9  项目阶段划分甘特图
# ============================================================
def fig_gantt():
    fig, ax = plt.subplots(figsize=(14, 8))
    ax.set_xlim(0, 11); ax.set_ylim(-0.8, 9.6)
    ax.set_title('项目阶段划分总览（甘特图）  2026.09 — 2027.07',
                 fontsize=17, fontweight='bold', pad=14, color='#1A237E')

    months = ['2026\n09', '2026\n10', '2026\n11', '2026\n12', '2027\n01',
              '2027\n02', '2027\n03', '2027\n04', '2027\n05', '2027\n06', '2027\n07']
    ax.set_xticks([i + 0.5 for i in range(11)])
    ax.set_xticklabels(months, fontsize=9)
    ax.set_yticks([])

    for i in range(12):
        ax.axvline(i, color='#CFD8DC', lw=0.8, zorder=0)
    ax.axvspan(2.0, 3.0, color='#ECEFF1', zorder=0)
    ax.text(2.5, 9.25, '软考中项', fontsize=9, ha='center', color='#78909C')
    for x, lab in ((7.5, '蓝桥杯'), (8.5, '嵌入式\n设计师')):
        ax.axvline(x, color='#C0392B', lw=1.4, ls='--', zorder=0, alpha=0.55)
        ax.text(x, 8.75, lab, fontsize=8.5, ha='center', color='#C0392B')

    tasks = [
        ('P0  硬件自检 + 工具链打通', 0.5, 1.0, 'blue'),
        ('P1  外设逐个点亮（5 个 demo）', 1.5, 0.55, 'blue'),
        ('P2  RTOS 任务架构', 3.0, 1.0, 'green'),
        ('P3  告警与故障诊断', 4.0, 1.0, 'green'),
        ('P4  通信与上云', 5.0, 1.0, 'orange'),
        ('P5  结构装配与安全链', 6.0, 0.5, 'orange'),
        ('P6  大屏与参赛材料', 6.5, 0.5, 'purple'),
        ('P7  参赛（报名 / 省赛 / 答辩）', 7.0, 3.0, 'red'),
    ]
    for i, (name, s, d, col) in enumerate(tasks):
        y = 8 - i
        fc, ec = C[col]
        ax.barh(y, d, left=s, height=0.52, color=fc, edgecolor=ec,
                linewidth=1.6, zorder=2)
        ax.text(s + 0.06, y, name, va='center', ha='left', fontsize=9.5,
                color='#111111', zorder=3)

    ms = [(1.2, '引脚表定稿'), (1.5, 'M1/M2/M3 跑通'), (2.05, '5 个外设 demo'),
          (4.0, '采集 1h 零复位'), (5.0, '故障注入全过'), (6.0, 'Modbus+MQTT 通'),
          (6.5, '整机联调完成'), (7.0, '参赛材料定稿'), (8.0, '省赛答辩')]
    ax.plot([m[0] for m in ms], [-0.35] * len(ms), marker='D', ls='none',
            markersize=7, color='#C0392B', zorder=4)
    for x, lab in ms:
        ax.text(x, -0.62, lab, fontsize=8, ha='center', va='top',
                color='#C0392B', rotation=18)
    ax.text(0.05, -0.35, '里程碑', fontsize=9.5, ha='left', va='center',
            color='#C0392B', fontweight='bold')

    ax.text(5.5, 9.55, '★ 关键约束：2027.9 秋招 —— 所有奖项必须在此之前拿到，否则写不进求职简历',
            fontsize=10, ha='center', color='#C0392B', fontweight='bold')
    ax.spines[['top', 'right', 'left']].set_visible(False)
    fig.savefig(os.path.join(OUT, 'fig9_gantt.png'), dpi=200,
                bbox_inches='tight', facecolor='white')
    plt.close(fig)
    print('OK fig9_gantt.png')


# ============================================================
# 图10  软件开发流程（自底向上分层）
# ============================================================
def fig_swflow():
    fig, ax = newfig('软件开发流程（自底向上，逐层验证）', 14, 9.5)

    layers = [
        ('L6', '验证层', '外设单元测试 → 集成测试 → 故障注入 → 72h 压力测试', 'red'),
        ('L5', '通信展示层', 'Modbus TCP 从站（502）· MQTT 客户端 · LVGL 界面 · 环形日志断网续传', 'purple'),
        ('L4', '应用逻辑层', '采集调度 · 告警状态机（迟滞/去抖）· 多源融合故障诊断 · 1Hz 加热控制', 'orange'),
        ('L3', '实时系统层', 'FreeRTOS 移植 · 8 任务划分 · 队列/互斥量/任务通知 · IWDG 看门狗', 'green'),
        ('L2', '外设驱动层', 'ILI9341 · SHT30 · DS18B20 · INA219 · W5500 · TF 卡(FatFS)', 'blue'),
        ('L1', '硬件抽象层', 'GPIO / SPI / I2C / UART 驱动 · 延时 · 串口重定向 · MPU 与 Cache 配置', 'gray'),
    ]
    y0, dy = 14.5, 13.5
    for i, (tag, name, content, col) in enumerate(layers):
        y = y0 + i * dy
        box(ax, 10, y, 15, 11, f'{tag}\n{name}', col, 10.5)
        box(ax, 59.5, y, 82, 11, content, col, 10, align='left')

    arrow(ax, (3.5, 61), (3.5, 18), color='#1A237E', lw=3, ms=22)
    ax.text(1.6, 39, '自底向上\n逐层验证', fontsize=10.5, rotation=90,
            ha='center', va='center', color='#1A237E', fontweight='bold')

    ax.text(50, 97, '顺序原则：上一层未通过验证，绝不进入下一层 —— 否则问题会指数级放大',
            fontsize=11.5, ha='center', color='#C0392B', fontweight='bold')
    ax.text(50, 2.5, '每一层都必须有可独立运行的验证手段（demo / 串口日志 / 万用表实测）',
            fontsize=10, ha='center', color='#546E7A', style='italic')
    save(fig, 'fig10_sw_flow.png')


# ============================================================
# 图11  硬件制作流程（4 板块）
# ============================================================
def fig_hwflow():
    fig, ax = newfig('硬件制作流程（4 个板块 · 12 个步骤）', 14, 9.5)

    lanes = [
        ('板块 A\n工具链与最小系统', 'blue',
         ['下载正点原子资料包 A 盘\n核对引脚分配表',
          '装 Keil MDK\n+ QSPI 下载算法',
          'M1 串口打印\n（须烧到外部 Flash）',
          'M2 RGBLED 三色闪\nPB4 / PE5 / PE6',
          'M3 TFT 显示\n（SPI 屏需自备驱动）']),
        ('板块 B\n控制电路搭建', 'green',
         ['面包板搭控制部分\n不接功率级',
          'I2C 扫描确认\n0x40(INA219) / 0x44(SHT30)']),
        ('板块 C\n功率回路搭建', 'orange',
         ['万用表查短路\n+ 确认 KSD9700 常闭',
          '弹簧端子串接：PPTC → KSD9700\n→ INA219 → 水泥电阻 → MOS',
          '先控制电后加热电\n量电流 ≈ 0.5A']),
        ('板块 D\n整机装配与验证', 'red',
         ['贴传感器到导热铝片\n+ 装盒 + 扎带理线',
          '72h 跑测 + 故障注入\n（断丝/短接/拔传感器/拔网线）']),
    ]
    y = 84
    idx = 1
    for name, col, steps in lanes:
        h = 17
        box(ax, 15, y, 28, h, name, col, 11)
        n = len(steps)
        w = (100 - 32 - 2) / n - 1.5
        for j, s in enumerate(steps):
            x = 32 + j * (w + 1.5)
            box(ax, x + w / 2, y, w, h - 2, f'{idx}.  {s}', col, 8.8)
            idx += 1
        if y > 20:
            arrow(ax, (15, y - h / 2), (15, y - h / 2 - 4.5), color='#78909C', lw=2)
        y -= 21.5

    ax.text(50, 97, '原则：先软件后硬件；先控制后功率；每一次上电前必须先查短路',
            fontsize=11.5, ha='center', color='#C0392B', fontweight='bold')
    save(fig, 'fig11_hw_flow.png')


# ============================================================
# 图12  软硬件联调流程
# ============================================================
def fig_integration():
    fig, ax = newfig('软硬件联调流程（8 个步骤，逐级放行）', 14, 10.5)

    steps = [
        ('只上控制电，不接加热回路', '串口日志正常；I2C 能扫到 0x40 与 0x44'),
        ('接入功率级，万用表串入回路量电流', '读数 ≈ 0.5A（单通道 10Ω 接 5V）'),
        ('INA219 读数与万用表比对（标定）', '偏差 < 5%；若反了检查 Vin+/Vin- 方向'),
        ('加热闭环测试：设定 30% 占空比', '温度曲线平滑上升，无跳变/无复位'),
        ('告警阈值测试：临时调低阈值', '三级告警依次触发，LED/蜂鸣器音调区分正确'),
        ('故障注入①：拔传感器 / 拔网线', '正确报传感器失效；数据写 TF 卡缓存'),
        ('故障注入②：断开加热丝 / 短接回路', '分别报「回路断路」与「过流短路」并切断'),
        ('过热保护演示：PWM 固定 100%', 'KSD9700 物理断开 → 报「硬件过热保护动作」并锁存'),
        ('72 小时连续跑测 + 数据记录', '零看门狗复位；生成温度/电流趋势图'),
    ]
    top, step = 90, 10.0
    for i, (a, b) in enumerate(steps):
        y = top - i * step
        box(ax, 28, y, 50, 8.4, f'步骤 {i+1}   {a}', 'blue', 9.8)
        box(ax, 79, y, 38, 8.4, b, 'green', 8.8)
        if i < len(steps) - 1:
            arrow(ax, (28, y - 4.2), (28, y - step + 4.2), color='#78909C', lw=1.7)
    ax.text(28, 96.5, '操作', fontsize=10.5, ha='center', fontweight='bold', color='#1A237E')
    ax.text(79, 96.5, '通过判据', fontsize=10.5, ha='center', fontweight='bold', color='#2E8B57')
    ax.text(50, 2, '★ 任一步骤未通过判据 → 停止，回到上一步排查；绝不带疑点往下走',
            fontsize=10.5, ha='center', color='#C0392B', fontweight='bold')
    save(fig, 'fig12_integration.png')


fig_gantt(); fig_swflow(); fig_hwflow(); fig_integration()
print('=== 策划书插图完成 ===')
