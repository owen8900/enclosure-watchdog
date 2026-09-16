/**
  ******************************************************************************
  * 文件    : xpt2046.h
  * 功能    : 2.8 寸 ILI9341 SPI 模块上的电阻触摸（XPT2046）驱动接口
  * 硬件    : MiniPro STM32H750 V1.5，4 条独立 GPIO 位翻转，不占硬件 SPI
  *            T_CS=PC3  T_CLK=PB14  T_DIN=PA3  T_DO=PC0
  * 说明    : 时序、命令字取自厂商软件 SPI 例程
  *            Demo_STM32F103RCT6_Software_SPI/HARDWARE/TOUCH/touch.c
  *            SCLK 上限约 2MHz，与 ILI9341 的 6MHz 不同，所以不共用 SPI2
  ******************************************************************************
  */

#ifndef __XPT2046_H
#define __XPT2046_H

#include "main.h"
#include <stdint.h>

/* 12 位 AD 的满量程 */
#define XPT2046_RAW_MAX     4095

/* 屏幕分辨率，必须与 ili9341.h 一致（不 include 是为了不依赖显示驱动） */
#define XPT2046_SCREEN_W    240
#define XPT2046_SCREEN_H    320

/* 「按下」判定的原始值窗口（没接 T_IRQ，只能靠值判断）：
 * 空闲时 Y 顶在 4095，按下时 Y 最大约 3820，100/4000 正好把两者分开。 */
#define XPT2046_VALID_MIN   100
#define XPT2046_VALID_MAX   4000

/* 标定参数：把原始 AD 值线性映射成像素。
 * raw_at_0 / raw_at_max = 第 0 像素和另一端处的原始值，两端填反了也能用。 */
typedef struct
{
    uint16_t x_raw_at_0;        /* 屏幕 X=0 处的原始值 */
    uint16_t x_raw_at_max;      /* 屏幕 X=239 处的原始值 */
    uint16_t y_raw_at_0;        /* 屏幕 Y=0 处的原始值 */
    uint16_t y_raw_at_max;      /* 屏幕 Y=319 处的原始值 */
    uint8_t  swap_xy;           /* 1 = 面板 X/Y 与屏幕相反，先交换再映射 */
} xpt2046_cal_t;

void                 xpt2046_init(void);
void                 xpt2046_scan(uint16_t *x, uint16_t *y);
uint8_t              xpt2046_read_raw(uint16_t *x, uint16_t *y);
uint8_t              xpt2046_read_screen(uint16_t *px, uint16_t *py);
uint8_t              xpt2046_read_screen_fast(uint16_t *px, uint16_t *py, uint16_t *rx, uint16_t *ry);
void                 xpt2046_set_cal(const xpt2046_cal_t *cal);
const xpt2046_cal_t *xpt2046_get_cal(void);
void                 xpt2046_set_log(void (*fn)(const char *msg));
void                 xpt2046_selftest(uint16_t seconds);
void                 xpt2046_verify(uint16_t seconds);
void                 xpt2046_wiring_test(void);

#endif /* __XPT2046_H */
