/**
  ******************************************************************************
  * 文件    : lcd_font.h
  * 功能    : 点阵字模表（8x16 ASCII）
  * 来源    : 从 LCDWiki MSP2807 厂商 FONT.H 的 asc2_1608[95][16] 原表抽出，
  *           未做任何改动。ref-vendor/MSP2807/extracted/.../HARDWARE/LCD/FONT.H
  * 说明    : 95 个字符，覆盖 0x20(空格) ~ 0x7E(~)，每个 16 字节 = 8 列 x 16 行，
  *           每字节一行，最高位是最左那个像素。
  ******************************************************************************
  */

#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#include <stdint.h>

#define LCD_FONT_W        8             /* 字宽（像素） */
#define LCD_FONT_H        16            /* 字高（像素） */
#define LCD_FONT_FIRST    0x20          /* 表里第一个字符 */
#define LCD_FONT_LAST     0x7E          /* 表里最后一个字符 */
#define LCD_FONT_COUNT    95

extern const uint8_t lcd_font_asc_1608[LCD_FONT_COUNT][LCD_FONT_H];

#endif /* __LCD_FONT_H */
