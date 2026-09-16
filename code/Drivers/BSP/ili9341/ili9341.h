/**
  ******************************************************************************
  * 文件    : ili9341.h
  * 功能    : LCDWiki 2.8 寸 SPI 模块（ILI9341，240x320）驱动接口
  * 硬件    : MiniPro STM32H750 V1.5，SPI2 + 3 根控制脚
  *            SCK=PB13  MOSI=PB15  CS=PB12  RST=PE0  DC=PE1
  *            背光 LED 直连 3.3V（常亮，未做 PWM 调光）
  * 说明    : 初始化序列取自厂商例程
  *            ref-vendor/MSP2807/.../Demo_STM32F429IGT6_Hardware_SPI/HARDWARE/LCD/lcd.c
  ******************************************************************************
  */

#ifndef __ILI9341_H
#define __ILI9341_H

#include "main.h"
#include "lcd_font.h"
#include <stdint.h>

/* 面板分辨率（竖屏 0° ） */
#define ILI9341_WIDTH    240
#define ILI9341_HEIGHT   320

/* RGB565 常用颜色 */
#define ILI9341_BLACK    0x0000
#define ILI9341_WHITE    0xFFFF
#define ILI9341_RED      0xF800
#define ILI9341_GREEN    0x07E0
#define ILI9341_BLUE     0x001F
#define ILI9341_YELLOW   0xFFE0
#define ILI9341_CYAN     0x07FF
#define ILI9341_MAGENTA  0xF81F
#define ILI9341_GRAY     0x8410
#define ILI9341_ORANGE   0xFD20

/* 背光板载 Q1，本模块接了 3.3V 常亮，留个宏方便以后改成 PWM */
#define ILI9341_BL_ON()   do { } while (0)
#define ILI9341_BL_OFF()  do { } while (0)

uint8_t  ili9341_init(void);                 /* 0 = 成功；非 0 = 初始化期间的错误计数 */
void     ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void     ili9341_fill(uint16_t color);       /* 整屏填充 */
void     ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void     ili9341_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void     ili9341_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void     ili9341_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void     ili9341_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void     ili9341_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void     ili9341_draw_char(uint16_t x, uint16_t y, char ch,
                           uint16_t fg, uint16_t bg, uint8_t scale);
void     ili9341_draw_string(uint16_t x, uint16_t y, const char *s,
                             uint16_t fg, uint16_t bg, uint8_t scale);
uint32_t ili9341_error_count(void);          /* SPI 传输失败次数（自检打印用） */
void     ili9341_error_reset(void);
void     ili9341_set_log(void (*fn)(const char *msg));   /* 注册日志函数（main.c 传 uart_puts 进来） */

/* 接线核对模式：逐根线慢速翻转 / 定值发送，配合万用表沿线核对。
 * 屏点亮之后这个函数可以不再调用。 */
void     ili9341_wiring_test(void);

#endif /* __ILI9341_H */
