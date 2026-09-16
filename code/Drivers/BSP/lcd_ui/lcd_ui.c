/**
  ******************************************************************************
  * 文件    : lcd_ui.c
  * 功能    : 控制柜面板界面（240x320 竖屏）
  ******************************************************************************
  */

#include "lcd_ui.h"
#include "ili9341.h"

/* 配色 */
#define UI_BG           ILI9341_BLACK
#define UI_TITLE_BG     0x0210          /* 深蓝 */
#define UI_LINE         0x4208          /* 中灰分隔线 */
#define UI_LABEL        ILI9341_GRAY
#define UI_VALUE        ILI9341_WHITE

/* 版面（竖屏 240 宽 x 320 高） */
#define UI_TITLE_Y      0
#define UI_TITLE_H      32
#define UI_R1_Y         44              /* 三行数值 */
#define UI_R2_Y         88
#define UI_R3_Y         132
#define UI_ROW_H        32
#define UI_ALARM_Y      184
#define UI_ALARM_H      60
#define UI_STATUS_Y     256
#define UI_SELF_Y       280

#define UI_LABEL_X      8
#define UI_VALUE_X      88
#define UI_UNIT_X       168
#define UI_VALUE_W      72              /* 数值格宽度（清底用） */
#define UI_BADGE_X      200             /* 右侧状态色块 */
#define UI_BADGE_W      32
#define UI_BADGE_H      24
#define UI_VALUE_SCALE  2

static uint16_t ui_strlen(const char *s)
{
    uint16_t n = 0;
    while (s[n] != '\0') n++;
    return n;
}

static char *ui_append(char *dst, const char *src)
{
    while (*src != '\0') { *dst++ = *src++; }
    *dst = '\0';
    return dst;
}

/* 等级 -> 颜色 */
static uint16_t ui_lvl_color(uint8_t lvl)
{
    if (lvl == LCD_UI_WARN)  return ILI9341_YELLOW;
    if (lvl == LCD_UI_ALARM) return ILI9341_ORANGE;
    if (lvl == LCD_UI_CRIT)  return ILI9341_RED;
    return ILI9341_GREEN;
}

/* 一行数值：清底 -> 画数值 -> 刷右侧色块 */
static void ui_row(uint16_t y, const char *val, uint8_t lvl)
{
    ili9341_fill_rect(UI_VALUE_X, y, UI_VALUE_W, UI_ROW_H, UI_BG);
    ili9341_draw_string(UI_VALUE_X, y, val, UI_VALUE, UI_BG, UI_VALUE_SCALE);
    ili9341_fill_rect(UI_BADGE_X, (uint16_t)(y + 4), UI_BADGE_W, UI_BADGE_H, ui_lvl_color(lvl));
}

/* 总告警条：整条上色 + 居中文字 */
static void ui_alarm_bar(uint8_t lvl)
{
    const char *t = "NORMAL";
    uint16_t color = ui_lvl_color(lvl);
    uint16_t tc = ILI9341_WHITE;
    uint16_t tw, x;

    if (lvl == LCD_UI_WARN)  { t = "WARNING";  tc = ILI9341_BLACK; }
    else if (lvl == LCD_UI_ALARM) { t = "ALARM"; }
    else if (lvl == LCD_UI_CRIT)  { t = "CRITICAL"; }

    ili9341_fill_rect(0, UI_ALARM_Y, ILI9341_WIDTH, UI_ALARM_H, color);

    tw = (uint16_t)(ui_strlen(t) * 8u * UI_VALUE_SCALE);
    x = (uint16_t)((ILI9341_WIDTH - tw) / 2u);
    ili9341_draw_string(x, (uint16_t)(UI_ALARM_Y + 14), t, tc, color, UI_VALUE_SCALE);
}

void lcd_ui_draw_static(void)
{
    ili9341_fill(UI_BG);

    /* 标题栏 */
    ili9341_fill_rect(0, UI_TITLE_Y, ILI9341_WIDTH, UI_TITLE_H, UI_TITLE_BG);
    ili9341_draw_string(8, 8, "CABINET GUARDIAN", ILI9341_WHITE, UI_TITLE_BG, 1);

    /* 三行标签与单位 */
    ili9341_draw_string(UI_LABEL_X, (uint16_t)(UI_R1_Y + 4), "TEMP", UI_LABEL, UI_BG, 1);
    ili9341_draw_string(UI_LABEL_X, (uint16_t)(UI_R2_Y + 4), "CURR", UI_LABEL, UI_BG, 1);
    ili9341_draw_string(UI_LABEL_X, (uint16_t)(UI_R3_Y + 4), "VOLT", UI_LABEL, UI_BG, 1);

    ili9341_draw_string(UI_UNIT_X, UI_R1_Y, "C", UI_LABEL, UI_BG, UI_VALUE_SCALE);
    ili9341_draw_string(UI_UNIT_X, UI_R2_Y, "A", UI_LABEL, UI_BG, UI_VALUE_SCALE);
    ili9341_draw_string(UI_UNIT_X, UI_R3_Y, "V", UI_LABEL, UI_BG, UI_VALUE_SCALE);

    /* 分隔线 */
    ili9341_draw_hline(0, 36, ILI9341_WIDTH, UI_LINE);
    ili9341_draw_hline(0, 176, ILI9341_WIDTH, UI_LINE);
    ili9341_draw_hline(0, 248, ILI9341_WIDTH, UI_LINE);
}

void lcd_ui_update(const lcd_ui_data_t *d)
{
    char buf[40];
    char *p;

    if (d == 0) return;

    ui_row(UI_R1_Y, d->temp, d->lvl_temp);
    ui_row(UI_R2_Y, d->curr, d->lvl_curr);
    ui_row(UI_R3_Y, d->volt, d->lvl_volt);
    ui_alarm_bar(d->alarm_level);

    /* 状态行 */
    p = buf;
    p = ui_append(p, "HEATER:");
    p = ui_append(p, d->heater_on ? "ON" : "OFF");
    p = ui_append(p, "  LOG:");
    p = ui_append(p, d->log_ok ? "OK" : "ERR");
    p = ui_append(p, "  CYC:");
    p = ui_append(p, d->cycle);
    ili9341_fill_rect(0, UI_STATUS_Y, ILI9341_WIDTH, 16, UI_BG);
    ili9341_draw_string(4, UI_STATUS_Y, buf, UI_VALUE, UI_BG, 1);

    /* 自检行 */
    p = buf;
    p = ui_append(p, "SD:");
    p = ui_append(p, d->sd_ok ? "OK" : "ERR");
    p = ui_append(p, "   SPI ERR:");
    p = ui_append(p, d->spi_err);
    ili9341_fill_rect(0, UI_SELF_Y, ILI9341_WIDTH, 16, UI_BG);
    ili9341_draw_string(4, UI_SELF_Y, buf, UI_VALUE, UI_BG, 1);
}
