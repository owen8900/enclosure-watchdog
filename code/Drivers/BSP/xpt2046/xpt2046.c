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
/* 前后两次采样的最大偏差（照厂商 TP_Read_XY2 的 ERR_RANGE = 50）：
 * 超过就认为抖了或没按稳，这一轮读数作废 */
#define XPT_ERR_RANGE    50

/* 位翻转的半周期延时。XPT2046 的 SCLK 上限约 2MHz（厂商例程也是 1us 一级地给），
 * 而 480MHz 内核一次 GPIO 翻转只要十几纳秒，必须自己拖慢。
 * 循环次数是估的、没上板量过：一次 volatile 空循环按 6~8 个周期算，40 次约 0.5us，
 * 半周期 0.5us -> SCLK 约 1MHz，离上限留一倍余量。触摸读数不是性能路径，慢点更稳。 */
#define XPT_HALF_CLK     40
/* 发完命令字等转换完成（厂商 delay_us(6)，手册 tCONV 上限 6us） */
#define XPT_CONV_LOOPS   240

/* ---- 四根线的电平操作 ---- */
#define TOUCH_CS_H    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port,  TOUCH_CS_Pin,  GPIO_PIN_SET)
#define TOUCH_CS_L    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port,  TOUCH_CS_Pin,  GPIO_PIN_RESET)
#define TOUCH_CLK_H   HAL_GPIO_WritePin(TOUCH_CLK_GPIO_Port, TOUCH_CLK_Pin, GPIO_PIN_SET)
#define TOUCH_CLK_L   HAL_GPIO_WritePin(TOUCH_CLK_GPIO_Port, TOUCH_CLK_Pin, GPIO_PIN_RESET)
#define TOUCH_DIN_H   HAL_GPIO_WritePin(TOUCH_DIN_GPIO_Port, TOUCH_DIN_Pin, GPIO_PIN_SET)
#define TOUCH_DIN_L   HAL_GPIO_WritePin(TOUCH_DIN_GPIO_Port, TOUCH_DIN_Pin, GPIO_PIN_RESET)
#define TOUCH_DO_IN   (HAL_GPIO_ReadPin(TOUCH_DO_GPIO_Port, TOUCH_DO_Pin) == GPIO_PIN_SET)

/* ---- 位翻转层 ---- */

/* 空转 loops 次。volatile 不能省，否则 -O3 会把这个空循环整个删掉。 */
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

    /* 补一个时钟把 BUSY 位推走。不补的话读回来的是 BUSY 打头的错位数据，
     * 表现是坐标整体偏小、还随按压力度乱跳。 */
    TOUCH_CLK_L;
    xpt_wait(XPT_HALF_CLK);
    TOUCH_CLK_H;
    xpt_wait(XPT_HALF_CLK);
    TOUCH_CLK_L;

    /* 读 16 位再右移 4 位得 12 位结果 —— 与厂商例程逐位一致。
     * 改成只读 12 位更漂亮，但那是自己引入的一层不确定性，不划算。 */
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

/* 两个数的差值（保证非负） */
static uint16_t xpt_diff(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

/* 值落在「按下」窗口里？没接 T_IRQ，这就是判断有没有按下的全部依据 */
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

/* ---- 标定与映射 ---- */

/* 默认标定是占位值：映射出来的像素坐标现在没有意义，必须上板量过再改。
 * 量法见 xpt2046_selftest() 的注释。 */
static xpt2046_cal_t s_cal =
{
    0,      /* x_raw_at_0   —— 触笔点屏幕左边（X=0）时的原始 X */
    4095,   /* x_raw_at_max —— 触笔点屏幕右边（X=239）时的原始 X */
    0,      /* y_raw_at_0   —— 触笔点屏幕顶部（Y=0）时的原始 Y */
    4095,   /* y_raw_at_max —— 触笔点屏幕底部（Y=319）时的原始 Y */
    0       /* swap_xy      —— 实测 X/Y 反了才置 1 */
};

/* 线性映射：raw_at_0 映射到 0 像素，raw_at_max 映射到 span-1 像素。
 * 两端填反了也能用（分子分母同时变号，结果仍是正确的反向映射），
 * 所以不需要单独的 invert 标志。越界一律钳到边界，不让 UI 拿到野坐标。 */
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

/* ---- 对外接口 ---- */

void xpt2046_init(void)
{
    /* 4 个脚的模式由 CubeMX 生成的 MX_GPIO_Init() 配好，这里只把空闲电平摆正：
     * 片选高（不选中）、时钟低、数据线低。 */
    TOUCH_CS_H;
    TOUCH_CLK_L;
    TOUCH_DIN_L;
}

/* 读一次物理坐标（两个轴各一次带滤波读数，约 0.3ms）。
 * 不做有效性判断 —— 松开触笔时给出的也是真实读数（通常跑到窗口外）。 */
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

/* 读一次并判断「算不算按下」：1 = 有效，0 = 没按下或读数抖了。
 * 判据两条（第二条照厂商 TP_Read_XY2）：X/Y 都落在有效窗口内，
 * 且连读两次的偏差都不超过 XPT_ERR_RANGE。一次调用约 0.6ms。 */
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

/* 读一次屏幕像素坐标：1 = 有效。标定参数不对时给出的坐标就是错的，
 * 所以第一次上板必须先用 xpt2046_selftest() 把 s_cal 量准。 */
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

/* ---- 日志（本工程不用 printf，省 Flash） ---- */

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

/* ---- 上板自检 ---- */

/* 每 200ms 打一行原始值，seconds 秒后自己结束（有界，不会卡住主流程）。
 * 两个用途：
 *   1) 证明触摸数据通路通了 —— 触笔按住时数值应随位置变化，
 *      松开时跑出窗口或乱跳；
 *   2) 取标定用的原始值 —— 触笔依次点在屏幕最左、最右、最上、最下，
 *      把那一刻的 X/Y 抄进 xpt2046_cal_t：
 *        屏幕左边 -> x_raw_at_0     屏幕右边 -> x_raw_at_max
 *        屏幕上边 -> y_raw_at_0     屏幕下边 -> y_raw_at_max
 *      抄完松开手再看几行，那些值就是「没按下」的实测范围，
 *      拿它去定 xpt2046.h 里的 XPT2046_VALID_MIN / MAX。 */
void xpt2046_selftest(uint16_t seconds)
{
    uint32_t t0   = HAL_GetTick();
    uint32_t last = t0;

    xpt_log("--- XPT2046 raw scan: press the stylus on the 4 edges ---\r\n");
    xpt_log("--- raw X / raw Y / verdict (TOUCH = inside the valid window) ---\r\n");

    while ((HAL_GetTick() - t0) < (uint32_t)seconds * 1000u)
    {
        uint16_t rx, ry;

        if ((HAL_GetTick() - last) < 200u)
        {
            continue;                       /* 打太快看不清 */
        }
        last = HAL_GetTick();

        xpt2046_scan(&rx, &ry);

        xpt_log("raw X=");
        xpt_log_u32(rx);
        xpt_log("  Y=");
        xpt_log_u32(ry);
        xpt_log((xpt_in_window(rx, ry) != 0) ? "  TOUCH" : "  idle");
        xpt_log("\r\n");
    }

    xpt_log("--- touch selftest done ---\r\n");
}

/* 接线核对：依次把 T_CS / T_CLK / T_DIN 以 1Hz 翻转 8 秒。
 * 万用表打到直流 20V 档，黑笔抵板子 GND，红笔点线头，应看到 0V <-> 3.3V 摆动。
 * 串口提示哪一根就是哪一根，一次只动一根。
 * T_DO 是输入脚，翻转不了 —— 那一根只能用蜂鸣档量通断。 */
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
