/**
  ******************************************************************************
  * 文件    : ds18b20.c
  * 功能    : DS18B20 单总线温度传感器驱动（1-Wire 位翻转，DQ = PC13 开漏输出）
  * 说明    : 微秒延时用 DWT 周期计数器，开机时拿 HAL 的 1ms 时基标定一次，
  *           所以不依赖 SystemCoreClock 有没有被更新过
  * 约定    : 每个时隙内关中断（几十微秒），免得 SysTick 把读时隙拉长、读错位
  ******************************************************************************
  */

#include "ds18b20.h"

#define OW_CMD_SKIP_ROM     0xCCu
#define OW_CMD_READ_ROM     0x33u
#define OW_CMD_CONVERT_T    0x44u
#define OW_CMD_READ_SCR     0xBEu

#define OW_CONVERT_MS       750u        /* 12 位转换最长 750ms */

static void (*s_log)(const char *) = 0;
static uint32_t s_ticks_per_us = 480u;  /* 标定前先按 480MHz 兜底 */
static uint8_t  s_dwt_ok;               /* DWT 计数器真的在走才用精确延时 */
static uint8_t  s_fail_shown;

void ds18b20_set_log(void (*log)(const char *s))
{
    s_log = log;
}

static void w_log(const char *s)
{
    if (s_log != 0)
    {
        s_log(s);
    }
}

static void w_log_u32(uint32_t v)
{
    char t[10];
    char c[2];
    int  i = 0;

    if (v == 0u)
    {
        w_log("0");
        return;
    }
    while (v > 0u && i < 10)
    {
        t[i++] = (char)('0' + (char)(v % 10u));
        v /= 10u;
    }
    c[1] = 0;
    while (i > 0)
    {
        c[0] = t[--i];
        w_log(c);
    }
}

static void w_log_hex2(uint8_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    char c[2];

    c[1] = 0;
    c[0] = hx[(v >> 4) & 0x0Fu];
    w_log(c);
    c[0] = hx[v & 0x0Fu];
    w_log(c);
}

/* 千分之一摄氏度打印成 25.625 这样的形式，串口字符串保持 ASCII */
static void w_log_milli(int32_t milli)
{
    uint32_t v;
    uint32_t frac;

    if (milli < 0)
    {
        w_log("-");
        v = (uint32_t)(-milli);
    }
    else
    {
        v = (uint32_t)milli;
    }

    w_log_u32(v / 1000u);
    w_log(".");
    frac = v % 1000u;
    w_log_u32(frac / 100u);
    w_log_u32((frac / 10u) % 10u);
    w_log_u32(frac % 10u);
}

/* DWT 周期计数器当微秒尺子。拿 HAL 的 1ms 时基标定，不读 SystemCoreClock */
static void dly_init(void)
{
    uint32_t t0;
    uint32_t c0;
    uint32_t c1;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    t0 = HAL_GetTick();
    c0 = DWT->CYCCNT;
    while ((HAL_GetTick() - t0) < 2u)       /* 量 2ms 再平均，取整误差减半 */
    {
    }
    c1 = DWT->CYCCNT;

    s_ticks_per_us = (c1 - c0) / 2000u;     /* 2ms = 2000us */
    s_dwt_ok = (s_ticks_per_us != 0u) ? 1u : 0u;
    if (s_dwt_ok == 0u)
    {
        s_ticks_per_us = 480u;              /* 计数器没走：退回粗糙延时，但保证会退出 */
    }
}

static void dly_us(uint32_t us)
{
    uint32_t start;
    uint32_t ticks;

    if (s_dwt_ok == 0u)
    {
        volatile uint32_t n = us * 30u;     /* 兜底：数量级够用就行，关键是能退出 */

        while (n > 0u)
        {
            n--;
        }
        return;
    }

    start = DWT->CYCCNT;
    ticks = us * s_ticks_per_us;
    while ((DWT->CYCCNT - start) < ticks)   /* CYCCNT 自己回绕，减法天然正确 */
    {
    }
}

static void ow_low(void)
{
    HAL_GPIO_WritePin(DS18B20_DQ_PORT, DS18B20_DQ_PIN, GPIO_PIN_RESET);
}

/* 开漏输出写 1 = 放开总线，靠板上 4.7K 上拉回高 */
static void ow_release(void)
{
    HAL_GPIO_WritePin(DS18B20_DQ_PORT, DS18B20_DQ_PIN, GPIO_PIN_SET);
}

static uint8_t ow_level(void)
{
    return (HAL_GPIO_ReadPin(DS18B20_DQ_PORT, DS18B20_DQ_PIN) == GPIO_PIN_SET) ? 1u : 0u;
}

/* 复位脉冲 + 应答检测：拉低 480us，放开后 70us 内器件把线拉低就是应答 */
static uint8_t ow_reset(void)
{
    uint8_t present;

    __disable_irq();
    ow_low();
    dly_us(480u);
    ow_release();
    dly_us(70u);
    present = (ow_level() == 0u) ? 1u : 0u;
    __enable_irq();
    dly_us(410u);

    return present;
}

/* 写时隙统一 70us：写 1 = 低 6us，写 0 = 低 60us */
static void ow_write_bit(uint8_t bit)
{
    __disable_irq();
    ow_low();
    if (bit != 0u)
    {
        dly_us(6u);
        ow_release();
        dly_us(64u);
    }
    else
    {
        dly_us(60u);
        ow_release();
        dly_us(10u);
    }
    __enable_irq();
}

/* 读时隙：拉低 2us 放开，第 15us 采样（手册要求在下降沿后 15us 内采） */
static uint8_t ow_read_bit(void)
{
    uint8_t bit;

    __disable_irq();
    ow_low();
    dly_us(2u);
    ow_release();
    dly_us(13u);
    bit = ow_level();
    __enable_irq();
    dly_us(55u);

    return bit;
}

static void ow_write_byte(uint8_t v)
{
    uint8_t i;

    for (i = 0u; i < 8u; i++)
    {
        ow_write_bit((uint8_t)((v >> i) & 0x01u));      /* 1-Wire 先发低位 */
    }
}

static uint8_t ow_read_byte(void)
{
    uint8_t v = 0u;
    uint8_t i;

    for (i = 0u; i < 8u; i++)
    {
        v |= (uint8_t)(ow_read_bit() << i);
    }

    return v;
}

/* Dallas/Maxim CRC8，多项式 X^8 + X^5 + X^4 + 1（0x8C 反写） */
static uint8_t ow_crc8(const uint8_t *d, uint8_t n)
{
    uint8_t crc = 0u;
    uint8_t i;
    uint8_t b;

    for (i = 0u; i < n; i++)
    {
        uint8_t x = d[i];

        for (b = 0u; b < 8u; b++)
        {
            uint8_t mix = (uint8_t)((crc ^ x) & 0x01u);

            crc >>= 1;
            if (mix != 0u)
            {
                crc ^= 0x8Cu;
            }
            x >>= 1;
        }
    }

    return crc;
}

uint8_t ds18b20_init(void)
{
    GPIO_InitTypeDef g = {0};

    /* PC13 自己配成开漏输出：CubeMX 里配没配都不影响（和 W5500 片选同一手法） */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = DS18B20_DQ_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;               /* 板上 R22 已有 4.7K 上拉，别再开内部上拉 */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DS18B20_DQ_PORT, &g);
    ow_release();

    dly_init();

    return (ow_reset() != 0u) ? 0u : 1u;
}

/* 读一次 ROM：1 = 总线上没人应答，2 = 读到了但 CRC 不过 */
static uint8_t read_rom_once(uint8_t rom[8])
{
    uint8_t i;

    if (ow_reset() == 0u)
    {
        return 1u;
    }
    ow_write_byte(OW_CMD_READ_ROM);
    for (i = 0u; i < 8u; i++)
    {
        rom[i] = ow_read_byte();
    }

    /* 对含 CRC 字节在内的 8 字节算 CRC，结果应为 0（残差形式）。
     * 踩过：这里原来写成 == rom[7]，等于要求「残差等于它自己」，恒不成立，
     * 于是 ROM 每次都被判 CRC 错（温度那条用的是残差式，所以没暴露）。 */
    return (ow_crc8(rom, 8u) == 0u) ? 0u : 2u;
}

/* 上电后第一次访问偶发失败过（温度读得好好的、ROM 却 CRC 不过），所以再试一次 */
uint8_t ds18b20_read_rom(uint8_t rom[8])
{
    uint8_t rc = read_rom_once(rom);

    if (rc != 0u)
    {
        rc = read_rom_once(rom);
    }

    return rc;
}

uint8_t ds18b20_read_temp(int16_t *raw)
{
    uint8_t sp[9];
    uint8_t i;

    if (raw == 0)
    {
        return 1u;
    }
    if (ow_reset() == 0u)
    {
        return 2u;
    }

    ow_write_byte(OW_CMD_SKIP_ROM);     /* 总线上只挂一只，跳过 ROM 匹配 */
    ow_write_byte(OW_CMD_CONVERT_T);
    HAL_Delay(OW_CONVERT_MS);           /* 转换期间靠外部上拉供电，等满最省事 */

    if (ow_reset() == 0u)
    {
        return 3u;
    }
    ow_write_byte(OW_CMD_SKIP_ROM);
    ow_write_byte(OW_CMD_READ_SCR);
    for (i = 0u; i < 9u; i++)
    {
        sp[i] = ow_read_byte();
    }
    if (ow_crc8(sp, 9u) != 0u)
    {
        return 4u;                      /* CRC 不过：线上有干扰或时序偏了 */
    }

    *raw = (int16_t)(((uint16_t)sp[1] << 8) | (uint16_t)sp[0]);
    return 0u;
}

/* 原始值是 1/16 °C 的有符号数，换成千分之一摄氏度 */
int32_t ds18b20_raw_to_milli(int16_t raw)
{
    return ((int32_t)raw * 1000) / 16;
}

void ds18b20_selftest(void)
{
    uint8_t rom[8];
    uint8_t i;
    uint8_t rc;
    int16_t raw;

    if (ds18b20_init() != 0u)
    {
        w_log("DS18B20: no answer on the bus (DQ=PC13, check plug)\r\n");
        return;
    }
    w_log("DS18B20: device present\r\n");

    rc = ds18b20_read_rom(rom);
    w_log("DS18B20: ROM=");
    for (i = 0u; i < 8u; i++)
    {
        w_log_hex2(rom[i]);
    }
    if (rc == 1u)
    {
        w_log("  (no presence pulse)\r\n");
    }
    else if (rc == 2u)
    {
        w_log("  (CRC ERR)\r\n");
    }
    else
    {
        w_log((rom[0] == 0x28u) ? "  (DS18B20)\r\n" : "  (first byte is not 0x28)\r\n");
    }

    /* ROM 认不出也继续测温：总线上只有一只时测温走 SKIP_ROM，不依赖 ROM */
    if (ds18b20_read_temp(&raw) != 0u)
    {
        w_log("DS18B20: read temp ERR\r\n");
        return;
    }
    w_log("DS18B20: T=");
    w_log_milli(ds18b20_raw_to_milli(raw));
    w_log(" C\r\n");
}

/* 主循环里周期调用。读失败只报一次，免得线掉了把串口刷满 */
void ds18b20_poll(void)
{
    int16_t raw;

    if (ds18b20_read_temp(&raw) != 0u)
    {
        if (s_fail_shown == 0u)
        {
            s_fail_shown = 1u;
            w_log("DS18B20: read ERR (bus lost?)\r\n");
        }
        return;
    }

    s_fail_shown = 0u;
    w_log("DS18B20: T=");
    w_log_milli(ds18b20_raw_to_milli(raw));
    w_log(" C\r\n");
}
