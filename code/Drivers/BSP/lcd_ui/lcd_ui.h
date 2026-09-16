/**
  ******************************************************************************
  * 文件    : lcd_ui.h
  * 功能    : 控制柜面板界面（240x320 竖屏）——静态框架 + 数据刷新
  * 硬件    : ILI9341 240x320，见 ili9341.h
  * 说明    : 数值先以字符串传入，等传感器接好由调用方格式化后填进来。
  *           屏幕上的文字目前只有 ASCII（8x16 字模），中文标签等 QSPI 字库那步再加。
  ******************************************************************************
  */

#ifndef __LCD_UI_H
#define __LCD_UI_H

#include "main.h"
#include <stdint.h>

/* 告警等级，和算法层的三级告警对齐 */
#define LCD_UI_OK      0
#define LCD_UI_WARN    1
#define LCD_UI_ALARM   2
#define LCD_UI_CRIT    3

typedef struct
{
    char    temp[10];       /* 例 "25.4"（不带单位） */
    char    curr[10];       /* 例 "0.42" */
    char    volt[10];       /* 例 "12.1" */
    char    cycle[12];      /* 心跳轮次 */
    char    spi_err[12];    /* 累计 SPI 错误数 */

    uint8_t lvl_temp;       /* 每路右侧色块：LCD_UI_OK / WARN / ALARM */
    uint8_t lvl_curr;
    uint8_t lvl_volt;

    uint8_t alarm_level;    /* 总告警等级，决定中间那条告警颜色 */
    uint8_t heater_on;
    uint8_t log_ok;
    uint8_t sd_ok;
} lcd_ui_data_t;

void lcd_ui_draw_static(void);                  /* 清屏 + 标题 + 标签 + 框架（只在开机画一次） */
void lcd_ui_repaint(void);                      /* 同上但不清屏：擦临时图形用 */
void lcd_ui_update(const lcd_ui_data_t *d);     /* 刷数值 / 色块 / 告警条 / 状态行 */

#endif /* __LCD_UI_H */
