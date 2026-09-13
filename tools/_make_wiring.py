# -*- coding: utf-8 -*-
"""接线工程图：接线总览图 + 引脚占用图"""
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle

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
    'idle':   ('#FAFAFA', '#B0BEC5'),
}


def box(ax, x, y, w, h, text, color='blue', fs=10.5, tc='#111111', lw=1.6,
        align='center', z=2):
    fc, ec = C[color]
    if align == 'left':
        ax.add_patch(FancyBboxPatch((x, y - h / 2), w, h,
                     boxstyle="round,pad=0.0,rounding_size=1.0",
                     linewidth=lw, facecolor=fc, edgecolor=ec, zorder=z))
        ax.text(x + 2.0, y, text, ha='left', va='center', fontsize=fs,
                color=tc, zorder=z + 1, linespacing=1.5)
    else:
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                     boxstyle="round,pad=0.0,rounding_size=1.0",
                     linewidth=lw, facecolor=fc, edgecolor=ec, zorder=z))
        ax.text(x, y, text, ha='center', va='center', fontsize=fs,
                color=tc, zorder=z + 1, linespacing=1.5)


# ============================================================
# 图13  接线总览图
# ============================================================
def wiring_overview():
    fig, ax = plt.subplots(figsize=(16, 10.5))
    ax.set_xlim(0, 100); ax.set_ylim(0, 100); ax.axis('off')
    ax.set_title('项目接线总览图 —— 什么模块接开发板哪个接口',
                 fontsize=18, fontweight='bold', pad=16, color='#1A237E')

    ax.text(50, 95.5,
            '※ 引脚号为推荐方案，落地前须对照《MiniPro H750 引脚分配表》核对（见 02 号文档）',
            fontsize=11, ha='center', color='#C0392B', fontweight='bold',
            bbox=dict(fc='#FDEDEC', ec='#C0392B', boxstyle='round,pad=0.5,rounding_size=0.6'))

    # 开发板
    box(ax, 17, 50, 26, 78,
        'STM32H750VB\n\n正点原子\nMiniPro H750\n\n480MHz M7\n1MB RAM\n16MB QSPI Flash',
        'green', 12)

    # 模块行：(模块名, 引脚文本, 总线分类色, 备注)
    rows = [
        ('SHT30 #1\n环境温度', 'PB8 (SCL)   PB9 (SDA)', 'purple', 'I2C1 · 3V3+GND · 0x44'),
        ('INA219\n电流 / 电压', 'PB8 (SCL)   PB9 (SDA)  共享', 'purple', 'I2C1 · 3V3+GND · 0x40'),
        ('SHT30 #2\n局部温度', 'PB10 (SCL)  PB11 (SDA)', 'purple', 'I2C2 · 3V3+GND'),
        ('DS18B20\n负载本体温度', 'PB12 (DQ)', 'teal', '单总线 · 4.7kΩ 上拉到 3V3'),
        ('2.4"/2.8" TFT\nILI9341 模块', 'PA5 SCK / PA6 MISO / PA7 MOSI', 'blue', 'CS PA4 · DC PA2 · RST PA3 · 背光 PB0 · 触摸CS PB1'),
        ('W5500\n以太网', 'PA5 / PA6 / PA7  共享', 'blue', '片选 PB7 · 中断 PC4 · 复位 PC6'),
        ('触摸控制器\n电阻 / 电容', 'PA5 / PA6 / PA7  共享（仅电阻屏）', 'blue', '电阻 XPT2046：CS=PB1 · IRQ=PC5 ｜ 电容 FT6236：走 I2C + INT'),
        ('ESP-01S\nWiFi', 'PD5 (TX)   PD6 (RX)', 'orange', 'USART2 · 建议独立 3.3V 供电'),
        ('IRLZ44N\n加热驱动', 'PA8  (TIM1_CH1)', 'red', '→ 100Ω 串阻 → G；10kΩ 下拉到 GND'),
        ('无源蜂鸣器\n告警音', 'PB15 (TIM12_CH2)', 'red', 'PWM 变调 · 模块 5V 供电'),
        ('告警 LED\n5mm 红灯', 'PC13', 'red', '5V → 220Ω → LED → PC13（低电平点亮）'),
    ]

    top, step = 88, 7.6
    for i, (name, pins, col, note) in enumerate(rows):
        y = top - i * step
        box(ax, 76, y, 40, 6.6, f'{name}\n{note}', col, 8.6)
        ax.add_patch(FancyArrowPatch((30, y), (56, y), arrowstyle='-|>',
                     mutation_scale=14, linewidth=2.0, color=C[col][1],
                     zorder=1, shrinkA=0, shrinkB=0))
        ax.text(43, y + 2.3, pins, fontsize=8.8, ha='center', va='bottom',
                color=C[col][1], fontweight='bold', zorder=4,
                bbox=dict(fc='white', ec='none', pad=0.8))

    # 图例
    leg = [('I2C 总线', 'purple'), ('SPI 共享总线', 'blue'),
           ('单总线', 'teal'), ('UART / 无线', 'orange'), ('PWM / GPIO', 'red')]
    for i, (t, col) in enumerate(leg):
        x = 6 + i * 18
        ax.add_patch(Rectangle((x, 1.2), 2.4, 2.4, facecolor=C[col][0],
                     edgecolor=C[col][1], linewidth=1.4))
        ax.text(x + 3.2, 2.4, t, fontsize=9.5, va='center', color='#37474F')

    fig.savefig(os.path.join(OUT, 'fig13_wiring.png'), dpi=200,
                bbox_inches='tight', facecolor='white')
    plt.close(fig)
    print('OK fig13_wiring.png')


# ============================================================
# 图14  引脚占用图
# ============================================================
def pin_map():
    fig, ax = plt.subplots(figsize=(17, 8))
    ax.set_xlim(0, 100); ax.set_ylim(0, 100); ax.axis('off')
    ax.set_title('开发板引脚占用图 —— 哪些脚能用、哪些禁占',
                 fontsize=18, fontweight='bold', pad=16, color='#1A237E')

    # 状态：禁用 / 板载 / 本项 / 空闲
    ST = {
        'qspi':  ('禁用', '#F5B7B1', '#922B21'),
        'board': ('板载', '#FCF3CF', '#B7950B'),
        'proj':  ('本项', '#D5F5E3', '#1E8449'),
        'idle':  ('空闲', '#F4F6F7', '#95A5A6'),
        'debug': ('调试', '#D6EAF8', '#1F618D'),
    }
    pins = {
        'PA': {0: ('KEY/WKUP', 'board'), 1: ('KEY0', 'board'), 2: ('TFT_DC', 'proj'),
               3: ('TFT_RST', 'proj'), 4: ('TFT_CS', 'proj'), 5: ('SPI1_SCK', 'proj'),
               6: ('SPI1_MISO', 'proj'), 7: ('SPI1_MOSI', 'proj'), 8: ('HEAT_PWM', 'proj'),
               9: ('USART1_TX', 'board'), 10: ('USART1_RX', 'board'), 11: ('-', 'idle'),
               12: ('-', 'idle'), 13: ('SWDIO', 'debug'), 14: ('SWCLK', 'debug'),
               15: ('KEY1', 'board')},
        'PB': {0: ('TFT_BL', 'proj'), 1: ('TOUCH_CS', 'proj'), 2: ('QSPI_CLK', 'qspi'),
               3: ('-', 'idle'), 4: ('RGB 红', 'board'), 5: ('-', 'idle'),
               6: ('QSPI_NCS', 'qspi'), 7: ('W5500_CS', 'proj'), 8: ('I2C1_SCL', 'proj'),
               9: ('I2C1_SDA', 'proj'), 10: ('I2C2_SCL', 'proj'), 11: ('I2C2_SDA', 'proj'),
               12: ('DS18B20', 'proj'), 13: ('-', 'idle'), 14: ('-', 'idle'),
               15: ('BUZZER', 'proj')},
        'PC': {0: ('-', 'idle'), 1: ('-', 'idle'), 2: ('-', 'idle'), 3: ('-', 'idle'),
               4: ('W5500_INT', 'proj'), 5: ('TOUCH_IRQ', 'proj'), 6: ('W5500_RST', 'proj'),
               7: ('-', 'idle'), 8: ('SDMMC', 'board'), 9: ('SDMMC', 'board'),
               10: ('SDMMC', 'board'), 11: ('SDMMC', 'board'), 12: ('SDMMC', 'board'),
               13: ('ALARM_LED', 'proj'), 14: ('LSE 晶振', 'qspi'), 15: ('LSE 晶振', 'qspi')},
        'PD': {0: ('-', 'idle'), 1: ('-', 'idle'), 2: ('SDMMC_CMD', 'board'),
               3: ('-', 'idle'), 4: ('-', 'idle'), 5: ('ESP_TX', 'proj'),
               6: ('ESP_RX', 'proj'), 7: ('-', 'idle'), 8: ('-', 'idle'),
               9: ('-', 'idle'), 10: ('-', 'idle'), 11: ('QSPI_IO0', 'qspi'),
               12: ('QSPI_IO1', 'qspi'), 13: ('QSPI_IO2', 'qspi'),
               14: ('-', 'idle'), 15: ('-', 'idle')},
        'PE': {0: ('-', 'idle'), 1: ('-', 'idle'), 2: ('QSPI_IO3', 'qspi'),
               3: ('-', 'idle'), 4: ('-', 'idle'), 5: ('RGB 绿', 'board'),
               6: ('RGB 蓝', 'board'), 7: ('-', 'idle'), 8: ('-', 'idle'),
               9: ('-', 'idle'), 10: ('-', 'idle'), 11: ('-', 'idle'),
               12: ('-', 'idle'), 13: ('-', 'idle'), 14: ('-', 'idle'), 15: ('-', 'idle')},
        'PH': {0: ('HSE 晶振', 'qspi'), 1: ('HSE 晶振', 'qspi')},
    }

    x0, cw = 7.5, 5.72
    row_h, y_top = 13.5, 90
    for r, (port, pm) in enumerate(pins.items()):
        y = y_top - r * row_h
        ax.text(3.4, y, port, fontsize=14, fontweight='bold', ha='center',
                va='center', color='#1A237E')
        for i in range(16):
            x = x0 + i * cw
            if r == 0:
                ax.text(x + cw / 2, y + row_h / 2 - 1.0, str(i), fontsize=9,
                        ha='center', va='center', color='#546E7A')
            if i not in pm:
                ax.add_patch(Rectangle((x, y - row_h / 2 + 1.2), cw - 0.5,
                             row_h - 2.6, facecolor='#FBFCFC',
                             edgecolor='#E5E8E8', linewidth=0.8))
                continue
            name, st = pm[i]
            fc, ec = ST[st][1], ST[st][2]
            ax.add_patch(Rectangle((x, y - row_h / 2 + 1.2), cw - 0.5,
                         row_h - 2.6, facecolor=fc, edgecolor=ec, linewidth=1.2))
            ax.text(x + cw / 2, y + 1.4, f'{port}{i}', fontsize=7.6, ha='center',
                    va='center', color='#37474F', fontweight='bold')
            ax.text(x + cw / 2, y - 2.2, name, fontsize=6.3, ha='center',
                    va='center', color=ec, rotation=0, linespacing=1.2)

    # 图例
    for i, (k, (lab, _, _)) in enumerate(ST.items()):
        x = 7.5 + i * 19
        ax.add_patch(Rectangle((x, 1.5), 2.6, 2.6, facecolor=ST[k][1],
                     edgecolor=ST[k][2], linewidth=1.4))
        ax.text(x + 3.4, 2.8, {'qspi': '禁占（QSPI/晶振）', 'board': '板载已占用',
                               'proj': '本项目使用', 'idle': '空闲可用',
                               'debug': '调试口保留'}[k], fontsize=9.5,
                va='center', color='#37474F')

    ax.text(50, 96.5,
            '★ 第一行是【板载 16MB QSPI Flash 与晶振】占用，绝不可挪用；程序本身就跑在该 Flash 上',
            fontsize=11, ha='center', color='#C0392B', fontweight='bold')
    ax.text(50, 93.4,
            '※ 表中"板载已占用"的判定依据见 02 号文档；落地前请用《MiniPro H750 引脚分配表》逐脚核对',
            fontsize=9.5, ha='center', color='#B7950B')

    fig.savefig(os.path.join(OUT, 'fig14_pinmap.png'), dpi=200,
                bbox_inches='tight', facecolor='white')
    plt.close(fig)
    print('OK fig14_pinmap.png')


wiring_overview()
pin_map()
print('=== 接线工程图完成 ===')
