# -*- coding: utf-8 -*-
"""生成技术说明文档用的图（3 张），输出到 docs/img/。
   刻意不用花哨样式：白底、细线、灰蓝配色，打印出来也清楚。"""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei"]
plt.rcParams["axes.unicode_minus"] = False

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "img")
os.makedirs(OUT, exist_ok=True)

EDGE = "#5b7c99"
FILL_APP = "#eaf1f7"
FILL_BSP = "#f3f6e8"
FILL_CORE = "#f7efe8"
GREY = "#444444"


def box(ax, x, y, w, h, text, fill="#ffffff", fs=10, bold=False, ec=EDGE):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.02",
                                linewidth=1.1, edgecolor=ec, facecolor=fill))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs,
            color=GREY, fontweight="bold" if bold else "normal", linespacing=1.5)


def arrow(ax, p1, p2, style="-|>", ls="-", color=EDGE, lw=1.2, text=None, tpos=0.5, toff=(0, 0.06)):
    ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle=style, linestyle=ls, color=color,
                                 linewidth=lw, mutation_scale=12, shrinkA=2, shrinkB=2))
    if text:
        mx = p1[0] + (p2[0] - p1[0]) * tpos + toff[0]
        my = p1[1] + (p2[1] - p1[1]) * tpos + toff[1]
        ax.text(mx, my, text, fontsize=9, color="#666666", ha="center")


# ============ 图 1：软件分层与数据流 ============
fig, ax = plt.subplots(figsize=(10, 6.4))
ax.set_xlim(0, 10); ax.set_ylim(0, 6.6); ax.axis("off")

ax.text(0.15, 6.25, "图 1  软件分层与数据流（谁调用谁，数据往哪走）", fontsize=13,
        fontweight="bold", color="#333333")

# 三个层次
box(ax, 0.15, 4.85, 9.7, 1.05, "", fill=FILL_CORE)
ax.text(0.35, 5.6, "Core 层（CubeMX 生成 + 手写调度）", fontsize=10, color="#8a6b52", fontweight="bold")
box(ax, 0.45, 5.0, 2.6, 0.5, "main() 上电自检 M1~M12", fs=9)
box(ax, 3.35, 5.0, 2.9, 0.5, "主循环：9 个时间片\nHAL_GetTick() 做节拍", fs=9)
box(ax, 6.55, 5.0, 3.0, 0.5, "CubeMX 外设初始化\nMX_GPIO/I2C2/SPI2/SDMMC1…", fs=9)

box(ax, 0.15, 3.05, 9.7, 1.45, "", fill=FILL_BSP)
ax.text(0.35, 4.3, "BSP 层（9 个驱动，各自带超时与错误计数）", fontsize=10, color="#6b7f3a", fontweight="bold")
for i, t in enumerate(["bsp_log\n串口日志出口", "i2c_bus\n扫描+收发原语", "sht30\n温湿度",
                       "ds18b20\n单总线温度", "w5500\n以太网 UDP", "ili9341 + lcd_ui\n屏与面板",
                       "xpt2046\n触摸"]):
    w = 1.24
    box(ax, 0.35 + i * 1.34, 3.2, w, 0.78, t, fs=8.2)

box(ax, 0.15, 1.25, 9.7, 1.5, "", fill=FILL_APP)
ax.text(0.35, 2.55, "App 层（4 个模块，零硬件调用 —— 所以能在 PC 上跑用例）", fontsize=10,
        color="#3f6b8a", fontweight="bold")
for i, t in enumerate(["filter\n中值/滑动平均\n最小二乘斜率", "diagnose\n六条判据\n一阶热模型残差",
                       "alarm_fsm\n五态 + 迟滞\n故障锁存", "heater\n1Hz 时间比例\nPWM 函数指针注入"]):
    box(ax, 0.45 + i * 2.4, 1.4, 2.15, 0.95, t, fs=8.6)

# 数据流
box(ax, 0.15, 0.2, 9.7, 0.75,
    "实测数据流：  传感器 ──BSP 读原始值──→ 物理量换算 ──→ filter ──→ diagnose ──→ alarm_fsm ──→ 屏显 / UDP 上报\n"
    "控制流：alarm_fsm 判定故障 ──→ heater_set_enable(0) 立即断输出；正常时 heater 按目标温度算占空比 ──→ PWM",
    fs=9, fill="#fafafa")

arrow(ax, (5.0, 4.85), (5.0, 4.55), text="调用")
arrow(ax, (5.0, 3.05), (5.0, 2.75), text="喂数据")
plt.tight_layout()
fig.savefig(os.path.join(OUT, "fig1_layers.png"), dpi=170, facecolor="white")
plt.close(fig)

# ============ 图 2：主循环时间片 ============
fig, ax = plt.subplots(figsize=(10, 5.2))
ax.set_xlim(0, 10); ax.set_ylim(0, 5.4); ax.axis("off")
ax.text(0.15, 5.15, "图 2  主循环时间片（裸机版，没有 RTOS 之前就是它）", fontsize=13,
        fontweight="bold", color="#333333")

rows = [
    ("触摸 XPT2046 轮询", "t_touch", 20, "TICK_TOUCH_MS"),
    ("RGB 轮闪", "t_led", 300, "TICK_LED_MS"),
    ("UDP 心跳发送", "t_udp", 1000, "W5500_UDP_HB_MS"),
    ("心跳：串口一行 + 写 SD 日志", "t_hb", 1000, "TICK_HB_MS"),
    ("DS18B20 读数", "t_ds", 2000, "TICK_DS18B20_MS"),
    ("SHT30 两个地址各读一次", "t_sht", 2000, "TICK_SHT30_MS"),
    ("I2C2 总线复扫（有变化才打印）", "t_i2c", 3000, "TICK_I2C_MS"),
    ("W5500 状态监视（原始读帧）", "t_w5", 5000, "TICK_W5500_MS"),
    ("面板刷新（约 0.3 s 一次重画）", "t_ui", 10000, "TICK_UI_MS"),
]
y = 4.72
for name, var, period, macro in rows:
    ax.text(0.2, y + 0.12, name, fontsize=9.2, color=GREY, va="center")
    ax.text(6.0, y + 0.12, f"{period} ms", fontsize=9.2, color="#8a3c3c", va="center",
            fontweight="bold", ha="right")
    ax.text(6.2, y + 0.12, macro, fontsize=8.2, color="#888888", va="center")
    # 条形长度按周期对数压缩，仅表示相对快慢
    import math
    w = 1.1 + 0.95 * math.log10(period / 20.0 + 1)
    ax.add_patch(FancyBboxPatch((7.55, y - 0.02), w, 0.26, boxstyle="round,pad=0.005,rounding_size=0.02",
                                linewidth=0.8, edgecolor=EDGE, facecolor="#dfe9f1"))
    y -= 0.44

ax.text(0.2, 0.52, "全部动作都是「到点才做、做完立刻返回」，没有一处 HAL_Delay 卡住主循环 ——\n"
                   "这是后面换 FreeRTOS 时能直接按周期搬成任务的前提。",
        fontsize=9, color="#555555", linespacing=1.6)
plt.tight_layout()
fig.savefig(os.path.join(OUT, "fig2_scheduler.png"), dpi=170, facecolor="white")
plt.close(fig)

# ============ 图 3：SHT30 一次读取的时序 ============
fig, ax = plt.subplots(figsize=(10, 4.6))
ax.set_xlim(0, 10); ax.set_ylim(0, 4.7); ax.axis("off")
ax.text(0.15, 4.45, "图 3  SHT30 一次测量的完整过程（驱动里就这几步）", fontsize=13,
        fontweight="bold", color="#333333")

steps = [
    (0.35, "① 发命令 0x2400\n2 字节（高重复度、\n不开时钟拉伸）", FILL_BSP),
    (2.35, "② 等 20 ms\n手册最大 15.5 ms，\n留余量", "#fdf3e3"),
    (4.35, "③ 读 6 字节\nT_MSB T_LSB T_CRC\nRH_MSB RH_LSB RH_CRC", FILL_APP),
    (6.35, "④ 两段 CRC8 校验\n多项式 0x31 / 初值 0xFF\n不对就返回错误码", "#fdeaea"),
    (8.35, "⑤ 定点换算\nT=-45+175·S/65535\nRH=100·S/65535", "#eef7ee"),
]
for x, t, f in steps:
    box(ax, x, 2.5, 1.5, 1.35, t, fs=8.4, fill=f)
for x in (1.85, 3.85, 5.85, 7.85):
    arrow(ax, (x + 0.02, 3.17), (x + 0.48, 3.17), lw=1.4)

ax.text(0.35, 1.9, "为什么用「不开时钟拉伸」的写法：\n"
                   "  开时钟拉伸时传感器会把 SCL 拉低直到测完，主机得一直等，HAL 超时容易误判；\n"
                   "  改成「发完命令就等固定 20 ms 再来读」，时序完全确定，也不占着总线。",
        fontsize=9.2, color="#555555", linespacing=1.7)
ax.text(0.35, 0.75, "同一份代码在 PC 上验过两件事：CRC8 对得上手册测例 CRC(0xBEEF)=0x92；\n"
                    "65536 个原始值的定点换算与浮点参考逐点比对无差异。",
        fontsize=9.2, color="#555555", linespacing=1.7)
plt.tight_layout()
fig.savefig(os.path.join(OUT, "fig3_sht30.png"), dpi=170, facecolor="white")
plt.close(fig)

print("生成完毕:", ", ".join(sorted(os.listdir(OUT))))
