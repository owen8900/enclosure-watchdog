/**
  ******************************************************************************
  * 文件    : ili9341.c
  * 功能    : LCDWiki 2.8 寸 SPI 模块（ILI9341，240x320）驱动实现
  * 硬件    : SCK=PB13  MOSI=PB15  CS=PB12  RST=PE0  DC=PE1  SPI2@6MHz Mode0
  * 说明    : 初始化序列（0xCF/0xED/0xE8/0xCB/0xF7/0xEA/0xC0~0xC7/0x36/0x3A/
  *            0xB1/0xB6/0xF2/0x26/0xE0/0xE1）取自厂商例程原值：
  *            ref-vendor/MSP2807/extracted/1-Demo/Demo_STM32/
  *            Demo_STM32F429IGT6_Hardware_SPI/HARDWARE/LCD/lcd.c 的 LCD_Init()
  * 约定    : 每次传输都带超时（100ms），失败计数，绝不无限阻塞
  ******************************************************************************
  */

#include "ili9341.h"
#include "spi.h"

/* 单次 SPI 发送的最大字节数：用静态缓冲，不占栈 */
#define LCD_TX_CHUNK      512
/* SPI 传输超时（毫秒） */
#define LCD_SPI_TIMEOUT   100

static uint8_t  s_tx[LCD_TX_CHUNK];
static uint32_t s_err;

/* ============================ 底层 ============================ */

/* 片选：assert=1 拉低选中，assert=0 释放 */
static inline void lcd_cs(uint8_t assert)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin,
                      assert ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* DC：0=命令，1=数据 */
static inline void lcd_dc(uint8_t data_mode)
{
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin,
                      data_mode ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 发一段字节；失败计数并返回 1 */
static uint8_t lcd_tx(const uint8_t *buf, uint16_t len)
{
    if (HAL_SPI_Transmit(&hspi2, (uint8_t *)buf, len, LCD_SPI_TIMEOUT) != HAL_OK)
    {
        s_err++;
        return 1;
    }
    return 0;
}

/* 写命令字节 */
static uint8_t lcd_wr_cmd(uint8_t cmd)
{
    uint8_t r;
    lcd_dc(0);
    lcd_cs(1);
    r = lcd_tx(&cmd, 1);
    lcd_cs(0);
    return r;
}

/* 写数据字节（可多字节） */
static uint8_t lcd_wr_data(const uint8_t *buf, uint16_t len)
{
    uint8_t r;
    lcd_dc(1);
    lcd_cs(1);
    r = lcd_tx(buf, len);
    lcd_cs(0);
    return r;
}

/* 命令 + 参数（参数表用数组传给，避免一长串散装调用） */
static void lcd_cmd_data(uint8_t cmd, const uint8_t *params, uint16_t n)
{
    lcd_wr_cmd(cmd);
    if (n > 0)
    {
        lcd_wr_data(params, n);
    }
}

/* ============================ 初始化 ============================ */

uint8_t ili9341_init(void)
{
    uint32_t err0 = s_err;

    /* 初始化序列参数表（厂商原值） */
    static const uint8_t p_cf[] = { 0x00, 0xC9, 0x30 };                             /* Power control B */
    static const uint8_t p_ed[] = { 0x64, 0x03, 0x12, 0x81 };                       /* Power on sequence */
    static const uint8_t p_e8[] = { 0x85, 0x10, 0x7A };                             /* Driver timing A */
    static const uint8_t p_cb[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };                 /* Power control A */
    static const uint8_t p_f7[] = { 0x20 };                                         /* Pump ratio */
    static const uint8_t p_ea[] = { 0x00, 0x00 };                                   /* Driver timing B */
    static const uint8_t p_c0[] = { 0x1B };                                         /* Power control 1 (VRH) */
    static const uint8_t p_c1[] = { 0x00 };                                         /* Power control 2 (BT) */
    static const uint8_t p_c5[] = { 0x30, 0x30 };                                   /* VCOM control 1 */
    static const uint8_t p_c7[] = { 0xB7 };                                         /* VCOM control 2 */
    static const uint8_t p_36[] = { 0x08 };                                         /* MADCTL：BGR，竖屏 0° */
    static const uint8_t p_3a[] = { 0x55 };                                         /* 像素格式：16bit RGB565 */
    static const uint8_t p_b1[] = { 0x00, 0x1A };                                   /* Frame rate control */
    static const uint8_t p_b6[] = { 0x0A, 0xA2 };                                   /* Display function control */
    static const uint8_t p_f2[] = { 0x00 };                                         /* 3Gamma 关闭 */
    static const uint8_t p_26[] = { 0x01 };                                         /* Gamma 曲线选择 */
    static const uint8_t p_e0[] = { 0x0F, 0x2A, 0x28, 0x08, 0x0E, 0x08, 0x54, 0xA9,
                                    0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00 };    /* 正极性 Gamma */
    static const uint8_t p_e1[] = { 0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56,
                                    0x3C, 0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F };    /* 负极性 Gamma */

    /* 硬件复位：拉低 10ms → 拉高 → 等 120ms（等内部稳压起来） */
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(120);

    /* 初始化序列 */
    lcd_cmd_data(0xCF, p_cf, sizeof p_cf);
    lcd_cmd_data(0xED, p_ed, sizeof p_ed);
    lcd_cmd_data(0xE8, p_e8, sizeof p_e8);
    lcd_cmd_data(0xCB, p_cb, sizeof p_cb);
    lcd_cmd_data(0xF7, p_f7, sizeof p_f7);
    lcd_cmd_data(0xEA, p_ea, sizeof p_ea);
    lcd_cmd_data(0xC0, p_c0, sizeof p_c0);
    lcd_cmd_data(0xC1, p_c1, sizeof p_c1);
    lcd_cmd_data(0xC5, p_c5, sizeof p_c5);
    lcd_cmd_data(0xC7, p_c7, sizeof p_c7);
    lcd_cmd_data(0x36, p_36, sizeof p_36);
    lcd_cmd_data(0x3A, p_3a, sizeof p_3a);
    lcd_cmd_data(0xB1, p_b1, sizeof p_b1);
    lcd_cmd_data(0xB6, p_b6, sizeof p_b6);
    lcd_cmd_data(0xF2, p_f2, sizeof p_f2);
    lcd_cmd_data(0x26, p_26, sizeof p_26);
    lcd_cmd_data(0xE0, p_e0, sizeof p_e0);
    lcd_cmd_data(0xE1, p_e1, sizeof p_e1);

    /* 整屏窗口（0..239 / 0..319） */
    {
        static const uint8_t x[] = { 0x00, 0x00, 0x00, 0xEF };
        static const uint8_t y[] = { 0x00, 0x00, 0x01, 0x3F };
        lcd_cmd_data(0x2A, x, sizeof x);
        lcd_cmd_data(0x2B, y, sizeof y);
    }

    lcd_wr_cmd(0x11);           /* 退出睡眠 */
    HAL_Delay(120);
    lcd_wr_cmd(0x29);           /* 开显示 */

    ili9341_fill(ILI9341_BLACK);    /* 清屏（先黑，刷色测试由调用方做） */

    return (uint8_t)(s_err - err0);
}

/* ============================ 绘图 ============================ */

void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t p[4];

    p[0] = (uint8_t)(x0 >> 8);
    p[1] = (uint8_t)(x0 & 0xFF);
    p[2] = (uint8_t)(x1 >> 8);
    p[3] = (uint8_t)(x1 & 0xFF);
    lcd_cmd_data(0x2A, p, 4);           /* 列地址范围 */

    p[0] = (uint8_t)(y0 >> 8);
    p[1] = (uint8_t)(y0 & 0xFF);
    p[2] = (uint8_t)(y1 >> 8);
    p[3] = (uint8_t)(y1 & 0xFF);
    lcd_cmd_data(0x2B, p, 4);           /* 页地址范围 */

    lcd_wr_cmd(0x2C);                   /* 准备写 GRAM */
}

void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t total;
    uint8_t  hi = (uint8_t)(color >> 8);
    uint8_t  lo = (uint8_t)(color & 0xFF);
    uint16_t i;

    if (w == 0 || h == 0)               return;
    if (x >= ILI9341_WIDTH)             return;
    if (y >= ILI9341_HEIGHT)            return;
    if ((uint32_t)x + w > ILI9341_WIDTH)  w = (uint16_t)(ILI9341_WIDTH  - x);
    if ((uint32_t)y + h > ILI9341_HEIGHT) h = (uint16_t)(ILI9341_HEIGHT - y);

    ili9341_set_window(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    /* 整个缓冲预填成同一个颜色，然后分块发 */
    for (i = 0; i < LCD_TX_CHUNK; i += 2)
    {
        s_tx[i]     = hi;
        s_tx[i + 1] = lo;
    }

    total = (uint32_t)w * (uint32_t)h * 2u;
    while (total > 0)
    {
        uint16_t n = (total > LCD_TX_CHUNK) ? (uint16_t)LCD_TX_CHUNK : (uint16_t)total;
        lcd_wr_data(s_tx, n);
        total -= n;
    }
}

void ili9341_fill(uint16_t color)
{
    ili9341_fill_rect(0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ili9341_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT) return;

    ili9341_set_window(x, y, x, y);
    s_tx[0] = (uint8_t)(color >> 8);
    s_tx[1] = (uint8_t)(color & 0xFF);
    lcd_wr_data(s_tx, 2);
}

/* ==================== 图形：线 / 矩形 ==================== */

void ili9341_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    ili9341_fill_rect(x, y, w, 1, color);           /* 走批量填充，比逐点快得多 */
}

void ili9341_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    ili9341_fill_rect(x, y, 1, h, color);
}

/* 空心矩形，边框 1 像素 */
void ili9341_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (w == 0 || h == 0) return;

    ili9341_draw_hline(x, y, w, color);
    ili9341_draw_hline(x, (uint16_t)(y + h - 1), w, color);
    ili9341_draw_vline(x, y, h, color);
    ili9341_draw_vline((uint16_t)(x + w - 1), y, h, color);
}

/* 任意两点连线：整数 Bresenham。步数有上限，不会死循环 */
void ili9341_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int32_t dx = (int32_t)x1 - (int32_t)x0;
    int32_t dy = (int32_t)y1 - (int32_t)y0;
    int32_t ax = (dx >= 0) ? dx : -dx;
    int32_t ay = (dy >= 0) ? dy : -dy;
    int32_t sx = (dx >= 0) ? 1 : -1;
    int32_t sy = (dy >= 0) ? 1 : -1;
    int32_t err = ax - ay;
    int32_t x = (int32_t)x0;
    int32_t y = (int32_t)y0;
    int32_t guard = ax + ay + 2;

    while (guard-- > 0)
    {
        ili9341_draw_pixel((uint16_t)x, (uint16_t)y, color);
        if (x == (int32_t)x1 && y == (int32_t)y1) break;

        if (2 * err > -ay) { err -= ay; x += sx; }
        if (2 * err <  ax) { err += ax; y += sy; }
    }
}

/* ==================== 文字（8x16 点阵） ==================== */

/* 画一个字符。scale=1 原尺寸 8x16，2 就 16x32。宽高越界或一行放不下就整体不画。
 * 逐行发送：每行的像素先在 s_tx 里按 scale 横向铺开，再纵向重复 scale 遍。 */
void ili9341_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t scale)
{
    const uint8_t *glyph;
    uint16_t w, h, cx, cy, i;
    uint8_t  bits;

    if (scale == 0) scale = 1;
    if (scale > 4)  scale = 4;
    if (ch < LCD_FONT_FIRST || ch > LCD_FONT_LAST) ch = '?';

    glyph = lcd_font_asc_1608[ch - LCD_FONT_FIRST];
    w = (uint16_t)(LCD_FONT_W * scale);
    h = (uint16_t)(LCD_FONT_H * scale);

    if ((uint32_t)x + w > ILI9341_WIDTH)  return;
    if ((uint32_t)y + h > ILI9341_HEIGHT) return;
    if ((uint32_t)w * 2u > LCD_TX_CHUNK)  return;        /* 一行装不进发送缓冲，不画 */

    ili9341_set_window(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    for (cy = 0; cy < LCD_FONT_H; cy++)
    {
        uint16_t rep;

        bits = glyph[cy];
        i = 0;
        for (cx = 0; cx < LCD_FONT_W; cx++)
        {
            uint16_t col = (bits & (uint16_t)(0x80u >> cx)) ? fg : bg;
            uint16_t k;

            for (k = 0; k < scale; k++)
            {
                s_tx[i]     = (uint8_t)(col >> 8);
                s_tx[i + 1] = (uint8_t)(col & 0xFF);
                i = (uint16_t)(i + 2);
            }
        }
        for (rep = 0; rep < scale; rep++)                /* 纵向放大：同一行发 scale 遍 */
        {
            lcd_wr_data(s_tx, i);
        }
    }
}

/* 画一串字符，画到屏幕右边就停（不会绕行） */
void ili9341_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale)
{
    uint16_t step;

    if (scale == 0) scale = 1;
    step = (uint16_t)(LCD_FONT_W * scale);

    while (*s != '\0')
    {
        if ((uint32_t)x + step > ILI9341_WIDTH) break;
        ili9341_draw_char(x, y, *s, fg, bg, scale);
        x = (uint16_t)(x + step);
        s++;
    }
}

/* ====================== 接线核对（配合万用表） ====================== */

static void (*s_log)(const char *msg) = 0;

/* 注册日志函数（main.c 把 uart_puts 传进来）；没注册就静默 */
void ili9341_set_log(void (*fn)(const char *msg))
{
    s_log = fn;
}

static void lcd_log(const char *msg)
{
    if (s_log != 0)
    {
        s_log(msg);
    }
}

/* 慢速翻转一根线：cycles 次，每次高 500ms / 低 500ms */
static void lcd_toggle_slow(GPIO_TypeDef *port, uint16_t pin, uint8_t cycles)
{
    uint8_t i;

    for (i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        HAL_Delay(500);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        HAL_Delay(500);
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);     /* 收尾回高：CS / RST 常态就是高 */
}

/* 持续发同一个字节 ms 毫秒：SCK 一直在翻转，MOSI 固定为该字节的每一位
 * 发 0x00 -> MOSI 恒低；发 0xFF -> MOSI 恒高。CS 保持高，屏不会理会这段数据。 */
static void lcd_spi_burst(uint8_t fill, uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    uint16_t i;

    for (i = 0; i < LCD_TX_CHUNK; i++)
    {
        s_tx[i] = fill;
    }
    while ((HAL_GetTick() - t0) < ms)               /* 有界循环，不会无限跑 */
    {
        (void)HAL_SPI_Transmit(&hspi2, s_tx, LCD_TX_CHUNK, LCD_SPI_TIMEOUT);
    }
}

/* 接线核对：5 步、共 40 秒。串口提示量哪根线，万用表用直流 20V 档，
 * 黑笔抵板子 GND，红笔点目标线（把线从屏上拔出来点公头） */
void ili9341_wiring_test(void)
{
    lcd_log("=== LCD wiring check: probe each wire with a multimeter ===\r\n");

    lcd_log("Step 1/5: CS  (PB12) toggle 1Hz for 8s -> should swing 0V <-> 3.3V\r\n");
    lcd_toggle_slow(TFT_CS_GPIO_Port, TFT_CS_Pin, 8);

    lcd_log("Step 2/5: RST (PE0)  toggle 1Hz for 8s -> should swing 0V <-> 3.3V\r\n");
    lcd_toggle_slow(TFT_RST_GPIO_Port, TFT_RST_Pin, 8);

    lcd_log("Step 3/5: DC  (PE1)  toggle 1Hz for 8s -> should swing 0V <-> 3.3V\r\n");
    lcd_toggle_slow(TFT_DC_GPIO_Port, TFT_DC_Pin, 8);

    lcd_log("Step 4/5: SPI burst 0x00 for 8s -> SCK ~1.6V, SDI(MOSI) = 0V\r\n");
    lcd_spi_burst(0x00, 8000);

    lcd_log("Step 5/5: SPI burst 0xFF for 8s -> SCK ~1.6V, SDI(MOSI) ~3.3V\r\n");
    lcd_spi_burst(0xFF, 8000);

    lcd_log("=== wiring check done ===\r\n");
}

/* ============================ 自检 ============================ */

uint32_t ili9341_error_count(void)
{
    return s_err;
}

void ili9341_error_reset(void)
{
    s_err = 0;
}
