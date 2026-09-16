/**
  ******************************************************************************
  * 文件    : xpt2046.c
  * 功能    : 2.8 寸 ILI9341 SPI 模块上的电阻触摸（XPT2046）驱动实现
  * 硬件    : T_CS=PC3  T_CLK=PB14  T_DIN=PA3  T_DO=PC0（位翻转 = 软件模拟 SPI）
  * 约定    : 每一层循环都是有界的，没有一处会无限阻塞
  ******************************************************************************
  */

#include "xpt2046.h"

/* XPT2046 控制字 = 起始位 1 + 通道 3 位 + 12bit 模式(0) + 差分(0) + 转换完断电(00) */
#define XPT_CMD_X        0xD0       /* 通道 101 = X */
#define XPT_CMD_Y        0x90       /* 通道 001 = Y */

/* 滤波：读 5 次，排序后丢掉最大最小各 1 个，中间 3 个取平均（照厂商 TP_Read_XOY） */
#define XPT_READ_TIMES   5
#define XPT_LOST_VAL     1
/* 前后两次采样的最大偏差（厂商 ERR_RANGE），超过即判抖动作废 */
#define XPT_ERR_RANGE    50

/* 位翻转半周期延时：40 次空循环约 0.5us，SCLK 约 1MHz（上限 2MHz 的一半） */
#define XPT_HALF_CLK     40
/* 发完命令字等转换完成（厂商 delay_us(6)，手册 tCONV 上限 6us） */
#define XPT_CONV_LOOPS   240

#define TOUCH_CS_H    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port,  TOUCH_CS_Pin,  GPIO_PIN_SET)
#define TOUCH_CS_L    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port,  TOUCH_CS_Pin,  GPIO_PIN_RESET)
#define TOUCH_CLK_H   HAL_GPIO_WritePin(TOUCH_CLK_GPIO_Port, TOUCH_CLK_Pin, GPIO_PIN_SET)
#define TOUCH_CLK_L   HAL_GPIO_WritePin(TOUCH_CLK_GPIO_Port, TOUCH_CLK_Pin, GPIO_PIN_RESET)
#define TOUCH_DIN_H   HAL_GPIO_WritePin(TOUCH_DIN_GPIO_Port, TOUCH_DIN_Pin, GPIO_PIN_SET)
#define TOUCH_DIN_L   HAL_GPIO_WritePin(TOUCH_DIN_GPIO_Port, TOUCH_DIN_Pin, GPIO_PIN_RESET)
#define TOUCH_DO_IN   (HAL_GPIO_ReadPin(TOUCH_DO_GPIO_Port, TOUCH_DO_Pin) == GPIO_PIN_SET)

/* 空转 loops 次（volatile 不能省，否则会被优化掉） */
static void xpt_wait(uint32_t loops)
{
    volatile uint32_t i;

    for (i = 0; i < loops; i++)
    {
    }
}

/* 发一个字节：MSB 先出，XPT2046 在时钟上升沿取样 */
static void xpt_write_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if ((b & 0x80u) != 0u)
        {
            TOUCH_DIN_H;
        }
        else
        {
            TOUCH_DIN_L;
        }
        b = (uint8_t)(b << 1);

        TOUCH_CLK_L;
        xpt_wait(XPT_HALF_CLK);
        TOUCH_CLK_H;
        xpt_wait(XPT_HALF_CLK);
    }
}

/* 读一次 AD 值：命令字 -> 等转换 -> 清 BUSY -> 读 16 位（只有高 12 位有效） */
static uint16_t xpt_read_ad(uint8_t cmd)
{
    uint16_t v = 0;
    uint8_t  i;

    TOUCH_CLK_L;
    TOUCH_DIN_L;
    TOUCH_CS_L;                         /* 选中触摸 IC */

    xpt_write_byte(cmd);
    xpt_wait(XPT_CONV_LOOPS);           /* 等这次转换出结果 */

    /* 补一个时钟把 BUSY 位推走，否则读回的是错位数据，坐标偏小且乱跳 */
    TOUCH_CLK_L;
    xpt_wait(XPT_HALF_CLK);
    TOUCH_CLK_H;
    xpt_wait(XPT_HALF_CLK);
    TOUCH_CLK_L;

    /* 读 16 位后右移 4 位得 12 位结果，与厂商例程一致 */
    for (i = 0; i < 16u; i++)
    {
        v = (uint16_t)(v << 1);

        TOUCH_CLK_L;
        xpt_wait(XPT_HALF_CLK);
        TOUCH_CLK_H;
        if (TOUCH_DO_IN)
        {
            v |= 1u;
        }
        xpt_wait(XPT_HALF_CLK);
    }

    v >>= 4;
    TOUCH_CS_H;                         /* 释放片选 */

    return v;
}

/* 带滤波的读数：读 5 次，排序后去掉最大最小各 1 个，中间 3 个求平均 */
static uint16_t xpt_read_filtered(uint8_t cmd)
{
    uint16_t buf[XPT_READ_TIMES];
    uint32_t sum = 0;
    uint8_t  i, j;

    for (i = 0; i < XPT_READ_TIMES; i++)
    {
        buf[i] = xpt_read_ad(cmd);
    }

    /* 5 个元素直接冒泡，不值得为它换个算法 */
    for (i = 0; i + 1u < XPT_READ_TIMES; i++)
    {
        for (j = (uint8_t)(i + 1u); j < XPT_READ_TIMES; j++)
        {
            if (buf[i] > buf[j])
            {
                uint16_t t = buf[i];
                buf[i] = buf[j];
                buf[j] = t;
            }
        }
    }

    for (i = XPT_LOST_VAL; i + XPT_LOST_VAL < XPT_READ_TIMES; i++)
    {
        sum += buf[i];
    }

    return (uint16_t)(sum / (XPT_READ_TIMES - 2u * XPT_LOST_VAL));
}

/* 两数之差的绝对值 */
static uint16_t xpt_diff(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

/* 值是否落在「按下」窗口内（没接 T_IRQ，只能靠值判断） */
static uint8_t xpt_in_window(uint16_t x, uint16_t y)
{
    if (x < XPT2046_VALID_MIN || x > XPT2046_VALID_MAX)
    {
        return 0;
    }
    if (y < XPT2046_VALID_MIN || y > XPT2046_VALID_MAX)
    {
        return 0;
    }
    return 1;
}

/* 标定值：2026-09-16 在这块屏上实测（标定向导取数 + 五点验收校方向）。
 * 这块面板的 raw Y 与屏幕 Y 相反 —— raw Y 大对应屏幕上方。
 * 数只对这块屏有效，换屏必须重量重校。 */
static xpt2046_cal_t s_cal =
{
    241,    /* x_raw_at_0   屏幕左 */
    3805,   /* x_raw_at_max 屏幕右 */
    3875,   /* y_raw_at_0   屏幕上（raw 大 = 上方） */
    256,    /* y_raw_at_max 屏幕下 */
    0       /* swap_xy      X/Y 未交换 */
};

/* 线性映射：raw_at_0 -> 0 像素，raw_at_max -> span-1 像素，越界钳到边界 */
static uint16_t xpt_map(uint16_t raw, uint16_t raw_at_0, uint16_t raw_at_max, uint16_t span)
{
    int32_t den = (int32_t)raw_at_max - (int32_t)raw_at_0;
    int32_t num;

    if (den == 0)
    {
        return 0;
    }

    num = ((int32_t)raw - (int32_t)raw_at_0) * (int32_t)(span - 1u);
    num = num / den;

    if (num < 0)
    {
        num = 0;
    }
    if (num > (int32_t)(span - 1u))
    {
        num = (int32_t)(span - 1u);
    }

    return (uint16_t)num;
}

void xpt2046_set_cal(const xpt2046_cal_t *cal)
{
    if (cal != 0)
    {
        s_cal = *cal;
    }
}

const xpt2046_cal_t *xpt2046_get_cal(void)
{
    return &s_cal;
}

void xpt2046_init(void)
{
    /* 引脚模式由 CubeMX 配好，这里只把空闲电平摆正 */
    TOUCH_CS_H;
    TOUCH_CLK_L;
    TOUCH_DIN_L;
}

/* 读一次物理坐标（约 0.3ms），不做有效性判断 */
void xpt2046_scan(uint16_t *x, uint16_t *y)
{
    if (x != 0)
    {
        *x = xpt_read_filtered(XPT_CMD_X);
    }
    if (y != 0)
    {
        *y = xpt_read_filtered(XPT_CMD_Y);
    }
}

/* 读一次并判断是否按下：1 = 有效。X/Y 都在窗口内且两次读数偏差不超 ERR_RANGE */
uint8_t xpt2046_read_raw(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1, x2, y2;

    xpt2046_scan(&x1, &y1);
    if (xpt_in_window(x1, y1) == 0)
    {
        return 0;
    }

    xpt2046_scan(&x2, &y2);
    if (xpt_in_window(x2, y2) == 0)
    {
        return 0;
    }
    if (xpt_diff(x1, x2) > XPT_ERR_RANGE)
    {
        return 0;
    }
    if (xpt_diff(y1, y2) > XPT_ERR_RANGE)
    {
        return 0;
    }

    if (x != 0)
    {
        *x = (uint16_t)((x1 + x2) / 2u);
    }
    if (y != 0)
    {
        *y = (uint16_t)((y1 + y2) / 2u);
    }
    return 1;
}

/* 读一次屏幕像素坐标：1 = 有效。标定不准时坐标就是错的 */
uint8_t xpt2046_read_screen(uint16_t *px, uint16_t *py)
{
    uint16_t rx, ry;

    if (xpt2046_read_raw(&rx, &ry) == 0)
    {
        return 0;
    }

    if (s_cal.swap_xy != 0)
    {
        uint16_t t = rx;
        rx = ry;
        ry = t;
    }

    if (px != 0)
    {
        *px = xpt_map(rx, s_cal.x_raw_at_0, s_cal.x_raw_at_max, XPT2046_SCREEN_W);
    }
    if (py != 0)
    {
        *py = xpt_map(ry, s_cal.y_raw_at_0, s_cal.y_raw_at_max, XPT2046_SCREEN_H);
    }
    return 1;
}

/* 定义在下面的验收段里 */
static void xpt_raw_to_px(uint16_t rx, uint16_t ry, uint16_t *px, uint16_t *py);

/* 单次读并映射成像素坐标：1 = 有效。不做两次一致校验（拖动时必然不一致），
 * 用于画线 / 跟手；要稳的点按判断用 xpt2046_read_screen()。
 * rx / ry 非空时把原始值带出去。 */
uint8_t xpt2046_read_screen_fast(uint16_t *px, uint16_t *py, uint16_t *rx, uint16_t *ry)
{
    uint16_t x, y;

    xpt2046_scan(&x, &y);
    if (xpt_in_window(x, y) == 0)
    {
        return 0;
    }

    if (rx != 0) { *rx = x; }
    if (ry != 0) { *ry = y; }

    xpt_raw_to_px(x, y, px, py);
    return 1;
}

/* 日志：本工程不用 printf，省 Flash */

static void (*s_log)(const char *msg) = 0;

void xpt2046_set_log(void (*fn)(const char *msg))
{
    s_log = fn;
}

static void xpt_log(const char *msg)
{
    if (s_log != 0)
    {
        s_log(msg);
    }
}

static void xpt_log_u32(uint32_t v)
{
    char buf[11];
    char tmp[10];
    int  i = 0;
    int  j;

    if (v == 0u)
    {
        buf[0] = '0';
        buf[1] = 0;
    }
    else
    {
        while (v > 0u && i < 10)
        {
            tmp[i] = (char)('0' + (int)(v % 10u));
            i++;
            v /= 10u;
        }
        for (j = 0; j < i; j++)
        {
            buf[j] = tmp[i - 1 - j];
        }
        buf[i] = 0;
    }

    xpt_log(buf);
}

/* 标定向导：点住一条边不动，固件在读数稳住时（采样数够且极差 <= XPT_STABLE_TOL，
 * 或撑到 XPT_PRESS_MAX 必捕获）自己锁平均值打一行 [CAPTURE]，一次按下对应一行。
 * 一轮四个点：LEFT / RIGHT / TOP / BOTTOM，四点齐了给出可抄进 s_cal 的块。
 * 受 seconds 秒上限约束。 */

#define XPT_STABLE_TOL    48u       /* 极差在这以内算读得干净（约 3 像素） */
#define XPT_PRESS_MIN     6u        /* 至少累计这么多次采样才允许早捕获 */
#define XPT_PRESS_MAX     16u       /* 撑到这么多次无论如何都捕获（16 x 50ms） */
#define XPT_SAMPLE_MS     50u       /* 采样间隔 */
#define XPT_PROMPT_MS     3000u     /* 空闲提示间隔 */
#define XPT_RELEASE_N     3u        /* 连续几次跑出窗口算松手 */

static const char *xpt_edge_name(uint8_t i)
{
    if (i == 0u) { return "LEFT"; }
    if (i == 1u) { return "RIGHT"; }
    if (i == 2u) { return "TOP"; }
    return "BOTTOM";
}

/* 汇总四点：四个标定值 + 交叉项（左右两点的 Y、上下两点的 X 应各自接近） */
static void xpt_calib_summary(const uint16_t cap[4][2], uint8_t round,
                              uint8_t have_idle, uint16_t xmin, uint16_t xmax,
                              uint16_t ymin, uint16_t ymax, uint16_t worst_spread)
{
    xpt_log("=== ROUND ");
    xpt_log_u32((uint32_t)round);
    xpt_log(" result: paste these into xpt2046.c ===\r\n");

    xpt_log("x_raw_at_0   (LEFT)   = ");
    xpt_log_u32(cap[0][0]);
    xpt_log("\r\n");
    xpt_log("x_raw_at_max (RIGHT)  = ");
    xpt_log_u32(cap[1][0]);
    xpt_log("\r\n");
    xpt_log("y_raw_at_0   (TOP)    = ");
    xpt_log_u32(cap[2][1]);
    xpt_log("\r\n");
    xpt_log("y_raw_at_max (BOTTOM) = ");
    xpt_log_u32(cap[3][1]);
    xpt_log("\r\n");

    xpt_log("cross check (each pair should be nearly equal):\r\n");
    xpt_log("  LEFT/RIGHT  Y: ");
    xpt_log_u32(cap[0][1]);
    xpt_log(" / ");
    xpt_log_u32(cap[1][1]);
    xpt_log("\r\n");
    xpt_log("  TOP/BOTTOM  X: ");
    xpt_log_u32(cap[2][0]);
    xpt_log(" / ");
    xpt_log_u32(cap[3][0]);
    xpt_log("\r\n");

    if (have_idle != 0u)
    {
        xpt_log("idle range (stylus up): X ");
        xpt_log_u32(xmin);
        xpt_log("..");
        xpt_log_u32(xmax);
        xpt_log("  Y ");
        xpt_log_u32(ymin);
        xpt_log("..");
        xpt_log_u32(ymax);
        xpt_log("\r\n");
    }

    /* 四点范围即「按下时」的真实范围，用来定窗口；空闲范围单独报 */
    {
        uint16_t xlo = cap[0][0], xhi = cap[0][0];
        uint16_t ylo = cap[0][1], yhi = cap[0][1];
        uint8_t  k;

        for (k = 1u; k < 4u; k++)
        {
            if (cap[k][0] < xlo) { xlo = cap[k][0]; }
            if (cap[k][0] > xhi) { xhi = cap[k][0]; }
            if (cap[k][1] < ylo) { ylo = cap[k][1]; }
            if (cap[k][1] > yhi) { yhi = cap[k][1]; }
        }

        xpt_log("pressed range (4 points): X ");
        xpt_log_u32(xlo);
        xpt_log("..");
        xpt_log_u32(xhi);
        xpt_log("  Y ");
        xpt_log_u32(ylo);
        xpt_log("..");
        xpt_log_u32(yhi);
        xpt_log("\r\n");

        if (have_idle != 0u)
        {
            xpt_log("idle Y rail = ");
            xpt_log_u32(ymin);
            xpt_log("   (idle X is meaningless: it wanders while X can overlap\r\n");
            xpt_log("  the left-edge pressed value, so never use X to gate the window)\r\n");

            if (ymin > XPT2046_VALID_MAX && yhi < XPT2046_VALID_MAX)
            {
                xpt_log("  -> current window (");
                xpt_log_u32(XPT2046_VALID_MIN);
                xpt_log("..");
                xpt_log_u32(XPT2046_VALID_MAX);
                xpt_log(") separates them: OK\r\n");
            }
            else
            {
                xpt_log("  -> window too tight: pressed Y or idle rail crosses ");
                xpt_log_u32(XPT2046_VALID_MAX);
                xpt_log("\r\n");
            }
        }
    }

    xpt_log("worst spread this round = ");
    xpt_log_u32(worst_spread);
    xpt_log((worst_spread <= XPT_STABLE_TOL) ? "  (clean)\r\n"
                                             : "  (noisy: hold the stylus more still, or redo)\r\n");

    xpt_log("=== end of round ===\r\n");
}

void xpt2046_selftest(uint16_t seconds)
{
    uint32_t t0          = HAL_GetTick();
    uint32_t last_sample = t0 - XPT_SAMPLE_MS;
    uint32_t last_prompt = t0;
    uint32_t sum_x       = 0u;
    uint32_t sum_y       = 0u;
    uint16_t cap[4][2];
    uint16_t pmin_x      = 0u;
    uint16_t pmax_x      = 0u;
    uint16_t pmin_y      = 0u;
    uint16_t pmax_y      = 0u;
    uint16_t worst       = 0u;
    uint16_t idle_xmin   = XPT2046_RAW_MAX;
    uint16_t idle_xmax   = 0u;
    uint16_t idle_ymin   = XPT2046_RAW_MAX;
    uint16_t idle_ymax   = 0u;
    uint8_t  have_idle   = 0u;
    uint8_t  n_press     = 0u;
    uint8_t  latched     = 0u;
    uint8_t  out_n       = 0u;
    uint8_t  step        = 0u;
    uint8_t  round       = 1u;

    xpt_log("=== XPT2046 calibration: press the stylus on the named edge ===\r\n");
    xpt_log("=== hold it still until [CAPTURE] shows up, then release ===\r\n");

    while ((HAL_GetTick() - t0) < (uint32_t)seconds * 1000u)
    {
        uint16_t rx, ry;
        uint16_t sx, sy;
        uint8_t  ready;

        if ((HAL_GetTick() - last_sample) < XPT_SAMPLE_MS)
        {
            continue;                       /* 还没到下一个采样点 */
        }
        last_sample = HAL_GetTick();
        xpt2046_scan(&rx, &ry);

        if (xpt_in_window(rx, ry) == 0)
        {
            /* 跑出窗口 = 没按下，顺手统计空闲范围 */
            if (rx < idle_xmin) { idle_xmin = rx; }
            if (rx > idle_xmax) { idle_xmax = rx; }
            if (ry < idle_ymin) { idle_ymin = ry; }
            if (ry > idle_ymax) { idle_ymax = ry; }
            have_idle = 1u;

            if (out_n < XPT_RELEASE_N)
            {
                out_n++;
            }
            if (out_n >= XPT_RELEASE_N)
            {
                if (latched != 0u)
                {
                    xpt_log("[RELEASE]\r\n");
                }
                latched = 0u;
                n_press = 0u;
            }

            if ((HAL_GetTick() - last_prompt) >= XPT_PROMPT_MS)
            {
                last_prompt = HAL_GetTick();
                xpt_log("waiting for ");
                xpt_log(xpt_edge_name(step));
                xpt_log(" edge   (");
                xpt_log_u32(((uint32_t)seconds * 1000u - (HAL_GetTick() - t0)) / 1000u);
                xpt_log("s left)\r\n");
            }
            continue;
        }

        out_n = 0u;                         /* 有读数落在窗口内 = 手按着 */

        if (n_press == 0u)                  /* 本次按下的第一个采样 */
        {
            sum_x   = rx;
            sum_y   = ry;
            pmin_x  = rx;
            pmax_x  = rx;
            pmin_y  = ry;
            pmax_y  = ry;
            n_press = 1u;
        }
        else
        {
            sum_x += rx;
            sum_y += ry;
            if (rx < pmin_x) { pmin_x = rx; }
            if (rx > pmax_x) { pmax_x = rx; }
            if (ry < pmin_y) { pmin_y = ry; }
            if (ry > pmax_y) { pmax_y = ry; }
            if (n_press < XPT_PRESS_MAX)
            {
                n_press++;
            }
        }

        if (latched != 0u)
        {
            continue;                       /* 这一点已经锁过，松手前不再出声 */
        }

        sx = (uint16_t)(pmax_x - pmin_x);
        sy = (uint16_t)(pmax_y - pmin_y);

        ready = 0u;
        if ((n_press >= XPT_PRESS_MIN) && (sx <= XPT_STABLE_TOL) && (sy <= XPT_STABLE_TOL))
        {
            ready = 1u;                     /* 读数干净，早捕获 */
        }
        if (n_press >= XPT_PRESS_MAX)
        {
            ready = 1u;                     /* 撑够了，吵也照样捕获 */
        }
        if (ready == 0u)
        {
            continue;
        }

        cap[step][0] = (uint16_t)(sum_x / (uint32_t)n_press);
        cap[step][1] = (uint16_t)(sum_y / (uint32_t)n_press);
        latched      = 1u;

        if (sx > worst) { worst = sx; }
        if (sy > worst) { worst = sy; }

        xpt_log("[CAPTURE ");
        xpt_log_u32((uint32_t)(step + 1u));
        xpt_log("/4] ");
        xpt_log(xpt_edge_name(step));
        xpt_log("  raw X=");
        xpt_log_u32(cap[step][0]);
        xpt_log("  Y=");
        xpt_log_u32(cap[step][1]);
        xpt_log("  (n=");
        xpt_log_u32((uint32_t)n_press);
        xpt_log("  spread X=");
        xpt_log_u32(sx);
        xpt_log(" Y=");
        xpt_log_u32(sy);
        xpt_log(")\r\n");

        step++;
        if (step >= 4u)
        {
            xpt_calib_summary(cap, round, have_idle, idle_xmin, idle_xmax,
                              idle_ymin, idle_ymax, worst);
            step  = 0u;
            round++;
            worst = 0u;
            xpt_log("--- round ");
            xpt_log_u32((uint32_t)round);
            xpt_log(": press LEFT again to redo, or wait out the timer ---\r\n");
        }
    }

    xpt_log("--- XPT2046 calibration done ---\r\n");
}

/* 五点验收：点住四个角加正中，固件把读数映射成像素后跟期望值比，直接给 PASS/FAIL。
 * 捕获判据与标定向导一致。 */

#define XPT_VERIFY_TOL   12u        /* 判定容差（像素），约 3 毫米 */

/* 原始值 -> 像素，swap_xy 处理同 xpt2046_read_screen() */
static void xpt_raw_to_px(uint16_t rx, uint16_t ry, uint16_t *px, uint16_t *py)
{
    uint16_t t;

    if (s_cal.swap_xy != 0)
    {
        t  = rx;
        rx = ry;
        ry = t;
    }

    if (px != 0)
    {
        *px = xpt_map(rx, s_cal.x_raw_at_0, s_cal.x_raw_at_max, XPT2046_SCREEN_W);
    }
    if (py != 0)
    {
        *py = xpt_map(ry, s_cal.y_raw_at_0, s_cal.y_raw_at_max, XPT2046_SCREEN_H);
    }
}

void xpt2046_verify(uint16_t seconds)
{
    static const char *vname[5] =
    {
        "TOP-LEFT", "TOP-RIGHT", "BOTTOM-LEFT", "BOTTOM-RIGHT", "CENTER"
    };
    static const uint16_t vexp_x[5] = { 0u, 239u, 0u, 239u, 120u };
    static const uint16_t vexp_y[5] = { 0u, 0u, 319u, 319u, 160u };

    uint32_t t0          = HAL_GetTick();
    uint32_t last_sample = t0 - XPT_SAMPLE_MS;
    uint32_t last_prompt = t0;
    uint32_t sum_x       = 0u;
    uint32_t sum_y       = 0u;
    uint16_t pmin_x      = 0u;
    uint16_t pmax_x      = 0u;
    uint16_t pmin_y      = 0u;
    uint16_t pmax_y      = 0u;
    uint8_t  n_press     = 0u;
    uint8_t  latched     = 0u;
    uint8_t  out_n       = 0u;
    uint8_t  step        = 0u;
    uint8_t  pass_n      = 0u;

    xpt_log("=== XPT2046 verify: 4 corners + center, hold still on each ===\r\n");
    xpt_log("=== look at the LCD: the side with the blue title bar is TOP ===\r\n");
    xpt_log("=== tolerance ");
    xpt_log_u32(XPT_VERIFY_TOL);
    xpt_log(" px ===\r\n");

    while ((HAL_GetTick() - t0) < (uint32_t)seconds * 1000u)
    {
        uint16_t rx, ry;
        uint16_t sx, sy;
        uint8_t  ready;

        if ((HAL_GetTick() - last_sample) < XPT_SAMPLE_MS)
        {
            continue;
        }
        last_sample = HAL_GetTick();
        xpt2046_scan(&rx, &ry);

        if (xpt_in_window(rx, ry) == 0)
        {
            if (out_n < XPT_RELEASE_N)
            {
                out_n++;
            }
            if (out_n >= XPT_RELEASE_N)
            {
                if (latched != 0u)
                {
                    xpt_log("[RELEASE]\r\n");
                }
                latched = 0u;
                n_press = 0u;
            }

            if ((HAL_GetTick() - last_prompt) >= XPT_PROMPT_MS)
            {
                last_prompt = HAL_GetTick();
                xpt_log("touch ");
                xpt_log(vname[step]);
                xpt_log("   (");
                xpt_log_u32(((uint32_t)seconds * 1000u - (HAL_GetTick() - t0)) / 1000u);
                xpt_log("s left)\r\n");
            }
            continue;
        }

        out_n = 0u;

        if (n_press == 0u)
        {
            sum_x   = rx;
            sum_y   = ry;
            pmin_x  = rx;
            pmax_x  = rx;
            pmin_y  = ry;
            pmax_y  = ry;
            n_press = 1u;
        }
        else
        {
            sum_x += rx;
            sum_y += ry;
            if (rx < pmin_x) { pmin_x = rx; }
            if (rx > pmax_x) { pmax_x = rx; }
            if (ry < pmin_y) { pmin_y = ry; }
            if (ry > pmax_y) { pmax_y = ry; }
            if (n_press < XPT_PRESS_MAX)
            {
                n_press++;
            }
        }

        if (latched != 0u)
        {
            continue;
        }

        sx = (uint16_t)(pmax_x - pmin_x);
        sy = (uint16_t)(pmax_y - pmin_y);

        ready = 0u;
        if ((n_press >= XPT_PRESS_MIN) && (sx <= XPT_STABLE_TOL) && (sy <= XPT_STABLE_TOL))
        {
            ready = 1u;
        }
        if (n_press >= XPT_PRESS_MAX)
        {
            ready = 1u;
        }
        if (ready == 0u)
        {
            continue;
        }

        {
            uint16_t avg_x = (uint16_t)(sum_x / (uint32_t)n_press);
            uint16_t avg_y = (uint16_t)(sum_y / (uint32_t)n_press);
            uint16_t px, py, dx, dy;

            xpt_raw_to_px(avg_x, avg_y, &px, &py);
            dx = xpt_diff(px, vexp_x[step]);
            dy = xpt_diff(py, vexp_y[step]);

            latched = 1u;
            if ((dx <= XPT_VERIFY_TOL) && (dy <= XPT_VERIFY_TOL))
            {
                pass_n++;
                xpt_log("[VERIFY ");
                xpt_log_u32((uint32_t)(step + 1u));
                xpt_log("/5] ");
                xpt_log(vname[step]);
                xpt_log("  raw ");
                xpt_log_u32(avg_x);
                xpt_log(",");
                xpt_log_u32(avg_y);
                xpt_log(" -> px ");
                xpt_log_u32(px);
                xpt_log(",");
                xpt_log_u32(py);
                xpt_log("  expect ");
                xpt_log_u32(vexp_x[step]);
                xpt_log(",");
                xpt_log_u32(vexp_y[step]);
                xpt_log("  PASS\r\n");
            }
            else
            {
                xpt_log("[VERIFY ");
                xpt_log_u32((uint32_t)(step + 1u));
                xpt_log("/5] ");
                xpt_log(vname[step]);
                xpt_log("  raw ");
                xpt_log_u32(avg_x);
                xpt_log(",");
                xpt_log_u32(avg_y);
                xpt_log(" -> px ");
                xpt_log_u32(px);
                xpt_log(",");
                xpt_log_u32(py);
                xpt_log("  expect ");
                xpt_log_u32(vexp_x[step]);
                xpt_log(",");
                xpt_log_u32(vexp_y[step]);
                xpt_log("  FAIL  dev ");
                xpt_log_u32(dx);
                xpt_log(",");
                xpt_log_u32(dy);
                xpt_log("\r\n");
            }

            step++;
            if (step >= 5u)
            {
                xpt_log("=== verify result: ");
                xpt_log_u32((uint32_t)pass_n);
                xpt_log("/5 PASS");
                xpt_log((pass_n == 5u) ? "  (calibration OK)\r\n"
                                       : "  (calibration needs redoing)\r\n");
                step   = 0u;
                pass_n = 0u;
                xpt_log("--- round done: touch TOP-LEFT again to redo ---\r\n");
            }
        }
    }

    xpt_log("--- XPT2046 verify done ---\r\n");
}

/* 接线核对：T_CS / T_CLK / T_DIN 逐根以 1Hz 翻转 8 秒，万用表直流档核对。
 * T_DO 是输入脚，只能用蜂鸣档量通断。 */
static void xpt_toggle(GPIO_TypeDef *port, uint16_t pin, uint8_t cycles, GPIO_PinState idle)
{
    uint8_t i;

    for (i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        HAL_Delay(500);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        HAL_Delay(500);
    }
    HAL_GPIO_WritePin(port, pin, idle);
}

void xpt2046_wiring_test(void)
{
    xpt_log("=== touch wiring check: probe the named wire with a multimeter ===\r\n");

    xpt_log("Step 1/3: T_CS  (PC3)  1Hz for 8s -> 0V <-> 3.3V\r\n");
    xpt_toggle(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, 8, GPIO_PIN_SET);

    xpt_log("Step 2/3: T_CLK (PB14) 1Hz for 8s -> 0V <-> 3.3V\r\n");
    xpt_toggle(TOUCH_CLK_GPIO_Port, TOUCH_CLK_Pin, 8, GPIO_PIN_RESET);

    xpt_log("Step 3/3: T_DIN (PA3)  1Hz for 8s -> 0V <-> 3.3V\r\n");
    xpt_toggle(TOUCH_DIN_GPIO_Port, TOUCH_DIN_Pin, 8, GPIO_PIN_RESET);

    xpt_log("T_DO (PC0) is an input, cannot toggle: check it with continuity mode\r\n");
    xpt_log("=== touch wiring check done ===\r\n");
}
