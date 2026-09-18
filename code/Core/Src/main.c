/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "sdmmc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "ili9341.h"
#include "lcd_ui.h"
#include "xpt2046.h"
#include "w5500.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 接线核对模式：1 = 上电先逐根线慢翻转，配合万用表核对；平时保持 0 */
#define LCD_WIRING_TEST 0

/* 触摸自检开关：
 * XPT_WIRING_TEST      1 = 触摸四线逐根慢翻转核对；平时 0
 * XPT_SELFTEST_MODE    1 = 标定向导  2 = 五点验收
 * XPT_SELFTEST_SECONDS > 0 = 上电跑 N 秒（阻塞），正式固件改回 0 */
#define XPT_WIRING_TEST        0
#define XPT_SELFTEST_MODE      2
#define XPT_SELFTEST_SECONDS   0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* FATFS/DIR/FIL 共约 1.2KB，main 的栈只有 1KB，必须放文件级静态 */
static FATFS s_fs;
static DIR   s_dir;
static FIL   s_fil;
/* dump / 取大小用的文件对象 */
static FIL   s_dbg;

static uint32_t g_cycle = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 极简串口输出，不用 printf，省几 KB Flash */
static void uart_puts(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 1000);
}

static void uart_put_u32(uint32_t v)
{
    char buf[12];
    char tmp[10];
    int  i = 0, j;

    if (v == 0u)
    {
        buf[0] = '0';
        buf[1] = '\0';
    }
    else
    {
        while (v > 0u && i < 10)
        {
            tmp[i++] = (char)('0' + (int)(v % 10u));
            v /= 10u;
        }
        for (j = 0; j < i; j++)
        {
            buf[j] = tmp[i - 1 - j];
        }
        buf[i] = '\0';
    }
    uart_puts(buf);
}

static void uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[9];
    int  i;

    for (i = 7; i >= 0; i--)
    {
        buf[i] = hex[v & 0xFu];
        v >>= 4;
    }
    buf[8] = '\0';
    uart_puts(buf);
}

/* 打一个字节的两位十六进制，只给诊断用 */
static void uart_put_hex2(uint8_t b)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[3];

    buf[0] = hex[(b >> 4) & 0x0Fu];
    buf[1] = hex[b & 0x0Fu];
    buf[2] = '\0';
    uart_puts(buf);
}

/* uint32 -> 十进制 ASCII，写进 out[]，返回字节数（不写结尾 '\0'） */
static int u32_to_str(uint32_t v, char *out)
{
    char tmp[10];
    int  i = 0;
    int  j;

    if (v == 0u)
    {
        out[0] = '0';
        return 1;
    }

    while (v > 0u)
    {
        tmp[i] = (char)('0' + (int)(v % 10u));
        i++;
        v /= 10u;
    }

    for (j = 0; j < i; j++)
    {
        out[j] = tmp[i - 1 - j];
    }

    return i;
}

/* 拼一行心跳： "cycle <n>\r\n"，返回字节数（不含结尾 '\0'，buf 至少 20 字节）。 */
static int build_cycle_line(uint32_t n, char *buf)
{
    int len;

    memcpy(buf, "cycle ", 6);
    len = 6;

    len += u32_to_str(n, buf + len);

    buf[len++] = '\r';
    buf[len++] = '\n';
    buf[len]   = '\0';

    return len;
}



/* 取文件字节数（只读打开→读大小→关闭）。打不开时返回 0xFFFFFFFF。 */
static uint32_t file_size_of(const char *path)
{
    FRESULT  fr;
    uint32_t sz;

    fr = f_open(&s_dbg, path, FA_READ);
    if (fr != FR_OK)
    {
        return 0xFFFFFFFFu;
    }
    sz = (uint32_t)f_size(&s_dbg);
    f_close(&s_dbg);

    return sz;
}

/* 把文件开头 max 字节 dump 到串口（没读卡器时的验证手段），定长发 */
static void file_dump_head(const char *path, uint32_t max)
{
    FRESULT  fr;
    UINT     br;
    uint32_t total = 0;
    uint32_t left;
    UINT     want;
    char     tmp[64];

    fr = f_open(&s_dbg, path, FA_READ);
    if (fr != FR_OK)
    {
        uart_puts("--- "); uart_puts(path);
        uart_puts(" : open FAILED fr="); uart_put_u32((uint32_t)fr); uart_puts("\r\n");
        return;
    }

    uart_puts("--- "); uart_puts(path); uart_puts(" size=");
    uart_put_u32((uint32_t)f_size(&s_dbg));
    uart_puts(" B, head ---\r\n");

    while (total < max)
    {
        left = max - total;
        want = (left < (uint32_t)sizeof(tmp)) ? (UINT)left : (UINT)sizeof(tmp);

        br = 0;
        fr = f_read(&s_dbg, tmp, want, &br);
        if (fr != FR_OK || br == 0)
        {
            break;
        }
        HAL_UART_Transmit(&huart1, (uint8_t *)tmp, (uint16_t)br, 1000);
        total += br;
    }

    uart_puts("\r\n--- dump end, "); uart_put_u32(total); uart_puts(" B shown ---\r\n");

    f_close(&s_dbg);
}

/* 面板数据。数值暂为占位值：SHT30 / INA219 / DS18B20 接好后换成实测转换结果 */
static lcd_ui_data_t s_ui;

/* 填数据并刷新面板。一次刷新约 0.3 秒，按周期调用，别每轮都刷 */
static void ui_refresh(uint32_t cycle, uint32_t spi_err, uint8_t sd_ok)
{
    char tmp[12];
    int  n;

    strcpy(s_ui.temp, "25.4");
    strcpy(s_ui.curr, "0.42");
    strcpy(s_ui.volt, "12.1");

    s_ui.lvl_temp    = LCD_UI_OK;
    s_ui.lvl_curr    = LCD_UI_WARN;      /* 故意留一路非正常，便于分辨三个色块 */
    s_ui.lvl_volt    = LCD_UI_OK;
    s_ui.alarm_level = LCD_UI_WARN;
    s_ui.heater_on   = 0;
    s_ui.log_ok      = 1;
    s_ui.sd_ok       = sd_ok;

    n = u32_to_str(cycle, tmp);
    tmp[n] = 0;
    strcpy(s_ui.cycle, tmp);

    n = u32_to_str(spi_err, tmp);
    tmp[n] = 0;
    strcpy(s_ui.spi_err, tmp);

    lcd_ui_update(&s_ui);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
/* M7：触摸轮询与画点 */

/* 各动作周期，时间片调度互不阻塞 */
#define TICK_LED_MS       300u      /* RGB 每色亮多久 */
#define TICK_TOUCH_MS      20u      /* 触摸扫描周期：50Hz 手感够，也不占 CPU */
#define TICK_HB_MS       1000u      /* 心跳：串口一行 + 追加进 SD 日志 */
#define TICK_UI_MS      10000u      /* 面板刷新（原来是每 10 轮，约 9 秒一次） */
#define TICK_W5500_MS    1000u      /* W5500 状态监视：值一变就打印，排查接线用 */

#define TOUCH_STATUS_Y    304u      /* 触摸状态行占用屏幕最底下 16 像素 */
#define TOUCH_MARK_COLOR  ILI9341_RED
#define TOUCH_MARK_TTL_MS 250u      /* 十字在手松开后亮多久就擦掉 */

/* 累计触摸次数 */
static uint32_t g_touch_n = 0u;

/* 十字的状态：g_mark_on=1 表示屏上还留着十字，g_mark_ms 是最后一次画的时间。
 * g_mark_x0..x1 / y0..y1 是这一轮画过的所有十字的包围盒（拖一笔会画一串），
 * 擦的时候按这个范围涂黑。 */
static uint8_t  g_mark_on = 0u;
static uint32_t g_mark_ms = 0u;
static uint8_t  g_touch_down = 0u;      /* 这一次触摸是否还按着 */
static uint16_t g_mark_x0 = 0u;
static uint16_t g_mark_x1 = 0u;
static uint16_t g_mark_y0 = 0u;
static uint16_t g_mark_y1 = 0u;
static uint16_t g_last_px = 0u;         /* 松手时的最终坐标，擦完十字要补回状态行 */
static uint16_t g_last_py = 0u;

/* 往 dst 尾部追加 src，返回追加的字节数 */
static int append_str(char *dst, const char *src)
{
    int n = 0;

    while (src[n] != '\0')
    {
        dst[n] = src[n];
        n++;
    }
    return n;
}

/* 在 (px,py) 画 9x9 十字，坐标先夹进屏幕（四周各伸 4 像素） */
static void touch_marker(uint16_t px, uint16_t py)
{
    uint16_t x = px;
    uint16_t y = py;

    if (x < 4u)                       { x = 4u; }
    if (x > (ILI9341_WIDTH - 5u))     { x = (uint16_t)(ILI9341_WIDTH - 5u); }
    if (y < 4u)                       { y = 4u; }
    if (y > (ILI9341_HEIGHT - 5u))    { y = (uint16_t)(ILI9341_HEIGHT - 5u); }

    ili9341_draw_hline((uint16_t)(x - 4u), y, 9u, TOUCH_MARK_COLOR);
    ili9341_fill_rect(x, (uint16_t)(y - 4u), 1u, 9u, TOUCH_MARK_COLOR);

    if (g_mark_on == 0u)                    /* 这一轮的第一笔：包围盒从它开始 */
    {
        g_mark_x0 = (uint16_t)(x - 4u);
        g_mark_x1 = (uint16_t)(x + 4u);
        g_mark_y0 = (uint16_t)(y - 4u);
        g_mark_y1 = (uint16_t)(y + 4u);
    }
    else
    {
        if ((uint16_t)(x - 4u) < g_mark_x0) { g_mark_x0 = (uint16_t)(x - 4u); }
        if ((uint16_t)(x + 4u) > g_mark_x1) { g_mark_x1 = (uint16_t)(x + 4u); }
        if ((uint16_t)(y - 4u) < g_mark_y0) { g_mark_y0 = (uint16_t)(y - 4u); }
        if ((uint16_t)(y + 4u) > g_mark_y1) { g_mark_y1 = (uint16_t)(y + 4u); }
    }

    g_mark_on = 1u;
    g_mark_ms = HAL_GetTick();
}

/* 底部状态行：最后一次的像素坐标 + 累计次数 */
static void touch_status(uint16_t px, uint16_t py)
{
    char buf[32];
    int  n = 0;

    n += append_str(buf + n, "TOUCH x=");
    n += u32_to_str(px, buf + n);
    n += append_str(buf + n, " y=");
    n += u32_to_str(py, buf + n);
    n += append_str(buf + n, "  n=");
    n += u32_to_str(g_touch_n, buf + n);
    buf[n] = '\0';

    ili9341_fill_rect(0u, TOUCH_STATUS_Y, ILI9341_WIDTH, 16u, ILI9341_BLACK);
    ili9341_draw_string(4u, TOUCH_STATUS_Y, buf, ILI9341_WHITE, ILI9341_BLACK, 1u);
}

/* 松手时串口打一行：像素坐标 + 对应原始值 + 累计次数 */
static void touch_log_line(uint16_t px, uint16_t py, uint16_t rx, uint16_t ry)
{
    uart_puts("TOUCH px=");
    uart_put_u32(px);
    uart_puts(" py=");
    uart_put_u32(py);
    uart_puts("  raw=");
    uart_put_u32(rx);
    uart_puts(",");
    uart_put_u32(ry);
    uart_puts("  n=");
    uart_put_u32(g_touch_n);
    uart_puts("\r\n");
}

/* 触摸轮询（每 TICK_TOUCH_MS 调一次）：按下画十字并跟手，松手把最终坐标记到状态行。
 * 十字跟手就说明触摸扫描没被 LED / 写卡 / 刷屏的延时卡住。 */
static void touch_poll(void)
{
    static uint8_t  down_before = 0u;
    static uint16_t last_x      = 0u;
    static uint16_t last_y      = 0u;
    static uint16_t last_rx     = 0u;
    static uint16_t last_ry     = 0u;
    uint16_t px = 0u, py = 0u;
    uint16_t rx = 0u, ry = 0u;
    uint16_t dx, dy;
    uint8_t  down;

    down = xpt2046_read_screen_fast(&px, &py, &rx, &ry);

    if (down == 0u)
    {
        if (down_before != 0u)                  /* 松手：记下最终坐标 */
        {
            down_before  = 0u;
            g_touch_down = 0u;
            g_last_px    = last_x;
            g_last_py    = last_y;
            touch_status(last_x, last_y);
            touch_log_line(last_x, last_y, last_rx, last_ry);
        }
        return;
    }

    if (down_before == 0u)                      /* 刚按下 */
    {
        down_before  = 1u;
        g_touch_down = 1u;
        g_touch_n++;
    }
    else
    {
        dx = (last_x > px) ? (uint16_t)(last_x - px) : (uint16_t)(px - last_x);
        dy = (last_y > py) ? (uint16_t)(last_y - py) : (uint16_t)(py - last_y);
        if ((dx < 3u) && (dy < 3u))
        {
            return;                             /* 没挪几个像素就不重画，省总线 */
        }
    }

    last_x  = px;
    last_y  = py;
    last_rx = rx;
    last_ry = ry;
    touch_marker(px, py);
}

/* 十字到点自己擦掉。屏的 SDO(MISO) 没接线、读不回显存，
 * 只能先把十字走过的范围涂黑，再把面板内容重画回去盖住被涂掉的部分。
 * 手还按着时不擦，免得重画这一下把触摸扫描挡住、漏掉下一次点按。 */
static void touch_marker_tick(void)
{
    if (g_mark_on == 0u)
    {
        return;
    }

    if (g_touch_down != 0u)                 /* 按着就先留着，松手再算时间 */
    {
        g_mark_ms = HAL_GetTick();
        return;
    }
    if ((HAL_GetTick() - g_mark_ms) < TOUCH_MARK_TTL_MS)
    {
        return;
    }

    g_mark_on = 0u;
    ili9341_fill_rect(g_mark_x0, g_mark_y0,
                      (uint16_t)(g_mark_x1 - g_mark_x0 + 1u),
                      (uint16_t)(g_mark_y1 - g_mark_y0 + 1u), ILI9341_BLACK);
    lcd_ui_repaint();
    ui_refresh(g_cycle, ili9341_error_count(), 1u);
    if (g_touch_n != 0u)
    {
        touch_status(g_last_px, g_last_py);     /* 底行不在面板范围内，单独补回 */
    }
}

int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
    /* M1：上电自检（USART1，板载 CH340C） */
    uart_puts("\r\n\r\n");
    uart_puts("========================================\r\n");
    uart_puts("  Cabinet Guardian - Cabinet Health Monitor\r\n");
    uart_puts("  Power-On Self Test (POST)\r\n");
    uart_puts("========================================\r\n");
    uart_puts("MCU        : STM32H750VBTx\r\n");
    uart_puts("SYSCLK     : "); uart_put_u32(HAL_RCC_GetSysClockFreq()); uart_puts(" Hz\r\n");
    uart_puts("HCLK (AXI) : "); uart_put_u32(HAL_RCC_GetHCLKFreq());    uart_puts(" Hz\r\n");
    uart_puts("PCLK1(APB1): "); uart_put_u32(HAL_RCC_GetPCLK1Freq());   uart_puts(" Hz\r\n");
    uart_puts("PCLK2(APB2): "); uart_put_u32(HAL_RCC_GetPCLK2Freq());   uart_puts(" Hz\r\n");
    uart_puts("UART       : 115200 8N1\r\n");
    uart_puts("----------------------------------------\r\n");
    uart_puts("RGB LED    : cycling RED -> GREEN -> BLUE\r\n\r\n");

    /* M4：TFT 屏自检（红 -> 绿 -> 蓝 各 0.4 秒）。串口字符串保持 ASCII */
    {
        uint8_t lcd_err;

#if LCD_WIRING_TEST
        ili9341_set_log(uart_puts);
        ili9341_wiring_test();
#endif

        uart_puts("TFT LCD    : ");
        lcd_err = ili9341_init();
        if (lcd_err == 0)
        {
            uart_puts("init OK");
        }
        else
        {
            uart_puts("init ERR x");
            uart_put_u32(lcd_err);
        }
        uart_puts("  SPI err total=");
        uart_put_u32(ili9341_error_count());
        uart_puts("\r\n");

        ili9341_fill(ILI9341_RED);
        HAL_Delay(400);
        ili9341_fill(ILI9341_GREEN);
        HAL_Delay(400);
        ili9341_fill(ILI9341_BLUE);
        HAL_Delay(400);
        ili9341_fill(ILI9341_BLACK);

        uart_puts("TFT color  : RED -> GREEN -> BLUE -> BLACK\r\n");
        /* M5：控制柜面板（数值先用占位值） */
        lcd_ui_draw_static();
        ui_refresh(0, ili9341_error_count(), 1);
        uart_puts("TFT UI     : panel drawn\r\n");
    }

    /* M6：触摸（XPT2046）自检。XPT_SELFTEST_MODE 选向导，受 XPT_SELFTEST_SECONDS
     * 秒上限约束，都是阻塞的，正式固件里秒数改回 0：
     *   1 = 标定向导（点四条边取标定值）  2 = 五点验收（四角加正中，PASS/FAIL） */
    {
        xpt2046_set_log(uart_puts);
        xpt2046_init();

        /* 开机打印实际生效的标定值，确认烧进去的是新固件 */
        {
            const xpt2046_cal_t *c = xpt2046_get_cal();

            uart_puts("XPT cal    : x ");
            uart_put_u32(c->x_raw_at_0);
            uart_puts("..");
            uart_put_u32(c->x_raw_at_max);
            uart_puts("  y ");
            uart_put_u32(c->y_raw_at_0);
            uart_puts("..");
            uart_put_u32(c->y_raw_at_max);
            uart_puts("  swap=");
            uart_put_u32(c->swap_xy);
            uart_puts("\r\n");
        }

#if XPT_WIRING_TEST
        uart_puts("TOUCH XPT  : wiring check\r\n");
        xpt2046_wiring_test();
#endif

#if XPT_SELFTEST_SECONDS > 0
    #if XPT_SELFTEST_MODE == 1
        xpt2046_selftest(XPT_SELFTEST_SECONDS);
    #elif XPT_SELFTEST_MODE == 2
        xpt2046_verify(XPT_SELFTEST_SECONDS);
    #endif
#endif
    }

    /* M8：W5500 自检。先只读版本寄存器 —— 读到 0x04 就说明 SPI 三线、
     * 片选、供电全对，不用急着联网。 */
    {
        uart_puts("W5500      : ");
        if (w5500_init() == 0)
        {
            uart_puts("init OK  VERSIONR=0x");
            uart_put_hex32((uint32_t)w5500_version());
            uart_puts("  PHYCFGR=0x");
            uart_put_hex32((uint32_t)w5500_read8(W5500_PHYCFGR, W5500_BSB_COMMON));
            uart_puts(w5500_link_up() ? "  LINK=up" : "  LINK=down");
            uart_puts("\r\n");
        }
        else
        {
            uart_puts("init ERR  version=0x");
            uart_put_hex32((uint32_t)w5500_version());
            uart_puts("  spi err total=");
            uart_put_u32(w5500_error_count());
            uart_puts("\r\n");
            {
                uint8_t p[8];
                uint8_t i, any = 0u, all = 0xFFu, idle, idle_fast;

                w5500_bus_probe(p, 8u);
                for (i = 0u; i < 8u; i++)
                {
                    any |= p[i];
                    all &= p[i];
                }
                uart_puts("             MISO probe: OR=0x");
                uart_put_hex32((uint32_t)any);
                uart_puts(" AND=0x");
                uart_put_hex32((uint32_t)all);
                if (any == 0u)
                {
                    uart_puts("  MISO stuck LOW  (no wire / no power / pulled low)\r\n");
                }
                else if (all == 0xFFu)
                {
                    uart_puts("  MISO stuck HIGH (no wire / floating)\r\n");
                }
                else
                {
                    uart_puts("  MISO toggling   (chip alive: look elsewhere)\r\n");
                }
                idle = w5500_miso_idle(&idle_fast);
                uart_puts("             MISO idle: fast=");
                uart_put_hex32((uint32_t)idle_fast);
                uart_puts(" settled=");
                uart_put_hex32((uint32_t)idle);
                uart_puts((idle == 0u)
                          ? "  (settled 0: something holds it low)\r\n"
                          : "  (settled 1: nobody drives it = floating or Hi-Z)\r\n");

                uart_puts("             probe bytes:");
                for (i = 0u; i < 8u; i++)
                {
                    uart_puts(" ");
                    uart_put_hex2(p[i]);
                }
                uart_puts("\r\n");

                /* 位翻转慢速 SPI 再读一次：绕过 SPI2 外设，
                 * 把"模块不搭话"和"SPI2 配置有问题"分开 */
                uart_puts("             via bitbang: VERSIONR=0x");
                uart_put_hex2(w5500_bitbang_read8(W5500_VERSIONR, W5500_BSB_COMMON));
                uart_puts("  RTR hi=0x");
                uart_put_hex2(w5500_bitbang_read8(W5500_RTR, W5500_BSB_COMMON));
                uart_puts(" lo=0x");
                uart_put_hex2(w5500_bitbang_read8((uint16_t)(W5500_RTR + 1u), W5500_BSB_COMMON));
                uart_puts("  (expect 04 / 07 / D0)\r\n");
            }
            uart_puts("             check: SCS=PA8 SCLK=PB13 MOSI=PB15 MISO=PB14 5V GND\r\n");
        }
    }

    /* M3：SD 卡 + FATFS
     * 顺序有讲究：CubeMX 使能 FATFS 后，不再由 sdmmc.c 初始化卡，改由 FATFS 的
     * BSP_SD_Init() 在首次 f_mount 时识别，所以卡信息必须挂载之后再读 ——
     * 读早了 hsd1 还是全 0。
     * 另：HAL_SD_GetCardInfo() 不填 CardSpeed，打印它会读到未初始化的栈内存。 */
    {
        HAL_SD_CardInfoTypeDef ci;
        FRESULT fr;
        DWORD   free_clusters = 0;
        FATFS  *fsp = &s_fs;

        /* 1. 挂载 —— 卡在这一步被识别 */
        fr = f_mount(&s_fs, "0:", 1);
        uart_puts("FATFS mount: ");
        if (fr != FR_OK)
        {
            uart_puts("FAILED  fr=");
            uart_put_u32((uint32_t)fr);
            uart_puts("   (13=FR_NO_FILESYSTEM, 3=FR_NOT_READY)\r\n");

            /* HAL 层可能还留着原因 */
            uart_puts("raw HAL    : state=");
            uart_put_u32((uint32_t)hsd1.State);
            uart_puts("  err=0x");
            uart_put_hex32(hsd1.ErrorCode);
            uart_puts("\r\n");
        }
        else
        {
            uart_puts("OK\r\n");

            /* 2. 卡信息（挂载之后才有效） */
            if (HAL_SD_GetCardInfo(&hsd1, &ci) != HAL_OK)
            {
                uart_puts("SD card    : GetCardInfo FAILED");
                uart_puts("\r\n");
            }
            else
            {
                /* 16GB 卡上 BlockNbr 乘 BlockSize 超过 32 位，必须用 64 位算。 */
                uint32_t cap_mb = (uint32_t)(((uint64_t)ci.BlockNbr *
                                              (uint64_t)ci.BlockSize)
                                             / (1024ULL * 1024ULL));
                uart_puts("SD card    : OK\r\n");
                uart_puts("SD type    : ");
                uart_put_u32(ci.CardType);    uart_puts("\r\n");
                uart_puts("SD version : ");
                uart_put_u32(ci.CardVersion); uart_puts("\r\n");
                /* Class 是 CSD 的 CCC 命令类位图，不是速度等级 */
                uart_puts("SD class   : ");
                uart_put_u32(ci.Class);       uart_puts("\r\n");
                uart_puts("SD blockNbr: ");
                uart_put_u32(ci.BlockNbr);    uart_puts("\r\n");
                uart_puts("SD blockSz : ");
                uart_put_u32(ci.BlockSize);   uart_puts("\r\n");
                uart_puts("SD capacity: ");
                uart_put_u32(cap_mb);         uart_puts(" MB"); uart_puts("\r\n");
            }

            /* 3. 剩余空间 */
            fr = f_getfree("0:", &free_clusters, &fsp);
            if (fr == FR_OK)
            {
                /* free_clusters 单位是簇；一簇 = s_fs.csize 个 512 字节扇区。 */
                uint32_t free_mb = (uint32_t)(((uint64_t)free_clusters *
                                              (uint64_t)s_fs.csize * 512ULL)
                                             / (1024ULL * 1024ULL));
                uart_puts("FATFS free : ");
                uart_put_u32(free_mb);
                uart_puts(" MB   (cluster = ");
                uart_put_u32((uint32_t)s_fs.csize);
                uart_puts(" sectors)\r\n");
            }
            else
            {
                uart_puts("FATFS free : FAILED  fr=");
                uart_put_u32((uint32_t)fr);
                uart_puts("\r\n");
            }

            /* 4. 列根目录 */
            {
                FILINFO  fno;
                uint32_t n = 0;

                fr = f_opendir(&s_dir, "0:/");
                if (fr != FR_OK)
                {
                    uart_puts("FATFS dir  : FAILED  fr=");
                    uart_put_u32((uint32_t)fr);
                    uart_puts("\r\n");
                }
                else
                {
                    uart_puts("FATFS dir  :\r\n");
                    for (;;)
                    {
                        fr = f_readdir(&s_dir, &fno);
                        if (fr != FR_OK || fno.fname[0] == 0) break;
                        uart_puts("  ");
                        if (fno.fattrib & AM_DIR) uart_puts("[DIR ] ");
                        else                      uart_puts("[FILE] ");
                        uart_puts(fno.fname);
                        uart_puts("\r\n");
                        n++;
                    }
                    f_closedir(&s_dir);
                    uart_puts("FATFS dir  : ");
                    uart_put_u32(n);
                    uart_puts(" entries\r\n");
                }
            }

            /* 5. 写文件再读回来比对，固件自己验证写入 */
            {
                UINT bw = 0, br = 0;
                static const char LINE[] = "Cabinet Guardian FATFS self test\r\n";
                char rd[sizeof(LINE)];

                fr = f_open(&s_fil, "0:/CG_TEST.TXT", FA_CREATE_ALWAYS | FA_WRITE);
                if (fr != FR_OK)
                {
                    uart_puts("FATFS write: FAILED (open) fr=");
                    uart_put_u32((uint32_t)fr);
                    uart_puts("\r\n");
                }
                else
                {
                    fr = f_write(&s_fil, LINE, sizeof(LINE) - 1, &bw);
                    if (fr == FR_OK) fr = f_sync(&s_fil);   /* 立刻落盘 */
                    f_close(&s_fil);

                    if (fr != FR_OK || bw != sizeof(LINE) - 1)
                    {
                        uart_puts("FATFS write: FAILED  fr=");
                        uart_put_u32((uint32_t)fr);
                        uart_puts("  wrote=");
                        uart_put_u32((uint32_t)bw);
                        uart_puts("\r\n");
                    }
                    else
                    {
                        uart_puts("FATFS write: ");
                        uart_put_u32((uint32_t)bw);
                        uart_puts(" bytes -> CG_TEST.TXT\r\n");

                        fr = f_open(&s_fil, "0:/CG_TEST.TXT", FA_READ);
                        if (fr == FR_OK)
                        {
                            fr = f_read(&s_fil, rd, sizeof(LINE) - 1, &br);
                            f_close(&s_fil);
                            rd[sizeof(LINE) - 1] = 0;
                            if (fr == FR_OK && br == sizeof(LINE) - 1 &&
                                memcmp(rd, LINE, sizeof(LINE) - 1) == 0)
                            {
                                uart_puts("FATFS read : OK, content matches\r\n");
                            }
                            else
                            {
                                uart_puts("FATFS read : MISMATCH\r\n");
                            }
                        }
                    }
                }
            }

            /* 6. 日志文件开头 dump 一次，看上一轮存了什么 */
            file_dump_head("0:/LOG.TXT", 200);
        }
    }


  /* USER CODE END 2 */

  /* M2/M7 时间片主循环的节拍。原来整轮用 HAL_Delay 串起来（光 RGB 三色就 0.9 秒），
     触摸扫描夹在里面会漏按。 */
  uint32_t t_led    = HAL_GetTick();
  uint32_t t_touch  = HAL_GetTick();
  uint32_t t_hb     = HAL_GetTick();
  uint32_t t_ui     = HAL_GetTick();
  uint32_t t_w5     = HAL_GetTick();
  uint8_t  led_step = 0u;

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* M2：RGB 轮闪（低电平点亮），每 TICK_LED_MS 换一种颜色 */
    if ((now - t_led) >= TICK_LED_MS)
    {
        t_led = now;
        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);      /* 全灭 */
        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);
        if (led_step == 0u)                    { HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET); }
        else if (led_step == 1u)               { HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET); }
        else                                   { HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET); }
        led_step = (uint8_t)((led_step + 1u) % 3u);
    }

    /* M7：触摸轮询（每 20ms 一次，跟手的关键） */
    if ((now - t_touch) >= TICK_TOUCH_MS)
    {
        t_touch = now;
        touch_poll();
        touch_marker_tick();
    }

    /* W5500 监视：只在值变化时打印，插着电动着线就能看串口反应 */
    if ((now - t_w5) >= TICK_W5500_MS)
    {
        static uint8_t last_ver = 0xFFu;
        uint8_t        ver;

        t_w5 = now;
        ver = w5500_read8(W5500_VERSIONR, W5500_BSB_COMMON);
        if (ver != last_ver)
        {
            last_ver = ver;
            uart_puts("W5500 watch: VERSIONR=0x");
            uart_put_hex32((uint32_t)ver);
            uart_puts((ver == 0x04u) ? "  OK\r\n" : "  (should be 0x04)\r\n");
        }
    }

    /* 心跳：串口一行 + 追加进 SD 日志 */
    if ((now - t_hb) >= TICK_HB_MS)
    {
        char line[20];
        int  n;

        t_hb = now;
        g_cycle++;
        n = build_cycle_line(g_cycle, line);
        uart_puts(line);

        {
            FRESULT fr;
            UINT    bw = 0;

            fr = f_open(&s_fil, "0:/LOG.TXT", FA_WRITE | FA_OPEN_APPEND);
            if (fr != FR_OK)
            {
                uart_puts("FATFS write: FAILED (open) fr=");
                uart_put_u32((uint32_t)fr);
                uart_puts("\r\n");
            }
            else
            {
                fr = f_write(&s_fil, line, n, &bw);
                if (fr != FR_OK || bw != n)
                {
                    uart_puts("FATFS write: FAILED  fr=");
                    uart_put_u32((uint32_t)fr);
                    uart_puts("  wrote=");
                    uart_put_u32((uint32_t)bw);
                    uart_puts("\r\n");
                }
                f_close(&s_fil);
            }
        }

        uart_puts("LOG size = ");
        uart_put_u32(file_size_of("0:/LOG.TXT"));
        uart_puts(" B\r\n");
    }

    /* 面板刷新：一次几十毫秒，周期放长 */
    if ((now - t_ui) >= TICK_UI_MS)
    {
        t_ui = now;
        ui_refresh(g_cycle, ili9341_error_count(), 1u);
    }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 120;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 先打印出错原因再停住 */
  uart_puts("\r\n*** FATAL: Error_Handler() ***\r\n");
  uart_puts("SD  err=0x"); uart_put_hex32(hsd1.ErrorCode);
  uart_puts("  state=");   uart_put_u32((uint32_t)hsd1.State);
  uart_puts("\r\n");

  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
