/**
  ******************************************************************************
  * 文件    : w5500.c
  * 功能    : W5500 以太网控制器（SPI）驱动实现
  * 说明    : 地址相位 3 字节 = addr[15:8] / addr[7:0] / (RWB<<7 | BSB)
  * 约定    : 每次传输都带超时（100ms），失败计数，不无限阻塞
  ******************************************************************************
  */

#include "w5500.h"
#include "spi.h"
#include "xpt2046.h"

/* Plan A 的硬前提：PB14 得是 SPI2_MISO，而它原来被触摸时钟占着。
 * 条件不满足就编译失败，免得编出一个 MISO 被触摸时钟占着的固件去插网线。 */
#if !defined(XPT_CLK_ON_PC2) || (XPT_CLK_ON_PC2 != 1)
  #error "W5500 needs PB14 as SPI2_MISO: set XPT_CLK_ON_PC2 to 1 (touch clock moves to PC2)"
#endif

#define W5500_SPI_TIMEOUT   100u

static uint32_t s_err;
static uint8_t  s_ver;
static uint8_t  s_tx[8];      /* 探针用：发 0xFF 看 MISO */

/* 片选，低有效。与屏共用 SPI2 时，同时只有被选中的那个设备会理会总线 */
static void w_cs(uint8_t assert)
{
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN,
                      assert ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* 发地址相位 3 字节 */
static uint8_t w_addr(uint16_t addr, uint8_t bsb, uint8_t read_op)
{
    uint8_t h[3];

    h[0] = (uint8_t)(addr >> 8);
    h[1] = (uint8_t)(addr & 0xFFu);
    h[2] = (uint8_t)((read_op ? 0x80u : 0x00u) | (bsb & 0x1Fu));

    if (HAL_SPI_Transmit(&hspi2, h, 3u, W5500_SPI_TIMEOUT) != HAL_OK)
    {
        s_err++;
        return 1u;
    }
    return 0u;
}

uint8_t w5500_read(uint16_t addr, uint8_t bsb, uint8_t *buf, uint16_t len)
{
    uint8_t err;

    w_cs(1u);
    err = w_addr(addr, bsb, 1u);
    if (err == 0u && len > 0u)
    {
        if (HAL_SPI_Receive(&hspi2, buf, len, W5500_SPI_TIMEOUT) != HAL_OK)
        {
            s_err++;
            err = 1u;
        }
    }
    w_cs(0u);
    return err;
}

uint8_t w5500_write(uint16_t addr, uint8_t bsb, const uint8_t *buf, uint16_t len)
{
    uint8_t err;

    w_cs(1u);
    err = w_addr(addr, bsb, 0u);
    if (err == 0u && len > 0u)
    {
        if (HAL_SPI_Transmit(&hspi2, (uint8_t *)buf, len, W5500_SPI_TIMEOUT) != HAL_OK)
        {
            s_err++;
            err = 1u;
        }
    }
    w_cs(0u);
    return err;
}

uint8_t w5500_read8(uint16_t addr, uint8_t bsb)
{
    uint8_t v = 0u;

    (void)w5500_read(addr, bsb, &v, 1u);
    return v;
}

void w5500_write8(uint16_t addr, uint8_t bsb, uint8_t val)
{
    (void)w5500_write(addr, bsb, &val, 1u);
}

/* 总线探针：CS 拉低后连发 8 个 0xFF（当作读操作），把 MISO 上看到的字节带出去。
 * 用来区分"完全没通信"的几种原因：恒低 = 没接/没电；恒高 = 浮空；有跳变 = 芯片活着。 */
void w5500_bus_probe(uint8_t *out, uint8_t n)
{
    uint8_t i;

    if (n > sizeof(s_tx))
    {
        n = (uint8_t)sizeof(s_tx);
    }
    for (i = 0; i < n; i++)
    {
        s_tx[i] = 0xFFu;
    }

    w_cs(1u);
    if (HAL_SPI_TransmitReceive(&hspi2, s_tx, out, n, W5500_SPI_TIMEOUT) != HAL_OK)
    {
        s_err++;
    }
    w_cs(0u);
}

uint8_t w5500_version(void)
{
    return s_ver;
}

/* MISO 开内部上拉后的静态电平。读完把 PB14 还原成 SPI2_MISO（AF5）。
 *
 * ⚠️ 切换引脚模式后必须等一会儿再读：40kΩ 的内上拉给线容充电要几微秒，
 * 切完立刻读会读到切换前的残留电平（第一版没等，把悬空误读成被拉低）。
 * 所以这里两种都读：*fast = 立刻读到的（残留，留着当证据），返回值 = 2ms 后稳定值。
 *
 * 稳定值 0 = 线上有东西在主动拉低；1 = 没人拉低（悬空或对方高阻）。 */
uint8_t w5500_miso_idle(uint8_t *fast)
{
    GPIO_InitTypeDef g = {0};
    uint8_t settled;

    g.Pin = GPIO_PIN_14;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);

    if (fast != 0)
    {
        *fast = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_SET) ? 1u : 0u;
    }

    HAL_Delay(2u);              /* 等上拉把线充起来 */

    settled = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_SET) ? 1u : 0u;

    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &g);

    return settled;
}

/* PHYCFGR 的 bit0 = 链路状态：插上网线、对端也在跑时读 1 */
uint8_t w5500_link_up(void)
{
    return (uint8_t)(w5500_read8(W5500_PHYCFGR, W5500_BSB_COMMON) & 0x01u);
}

uint32_t w5500_error_count(void)
{
    return s_err;
}

/* ---------------------------------------------------------------
 * 位翻转慢速 SPI（只用于诊断）
 * 绕开 SPI2 外设，用纯 GPIO 按 W5500 的时序读 VERSIONR：
 * 模式 0，MOSI 上升沿被从机采样，MISO 上升沿输出。
 * 读 MISO 时开内部上拉 —— 芯片不驱动时读成 1（跟"被拉低"分开）。
 * --------------------------------------------------------------- */
static void bb_delay(void)
{
    volatile uint32_t i;

    for (i = 0u; i < 700u; i++)     /* 半周期约几微秒；W5500 不挑速度 */
    {
    }
}

static void bb_pin_mode(uint8_t data_mode)
{
    GPIO_InitTypeDef g = {0};

    if (data_mode != 0u)
    {
        /* SCK / MOSI 输出 */
        g.Pin = GPIO_PIN_13 | GPIO_PIN_15;
        g.Mode = GPIO_MODE_OUTPUT_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &g);

        /* MISO 输入 + 上拉 */
        g.Pin = GPIO_PIN_14;
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOB, &g);
    }
    else
    {
        /* 还原成 SPI2 的 AF5，别把总线留在位翻转状态 */
        g.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        g.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &g);
    }
}

static void bb_out_bit(uint8_t bit)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, (bit != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    bb_delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);        /* 上升沿：从机采样 */
    bb_delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    bb_delay();
}

static uint8_t bb_in_bit(void)
{
    uint8_t bit;

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);        /* 上升沿：从机送数据 */
    bb_delay();
    bit = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_SET) ? 1u : 0u;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    bb_delay();

    return bit;
}

static void bb_out_byte(uint8_t val)
{
    uint8_t i;

    for (i = 0u; i < 8u; i++)
    {
        bb_out_bit((uint8_t)((val >> (7u - i)) & 1u));
    }
}

uint8_t w5500_bitbang_version(void)
{
    uint8_t v = 0u;
    uint8_t i;

    bb_pin_mode(1u);                        /* 切到位翻转用的方向 */
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_RESET);
    bb_delay();

    bb_out_byte(0x00u);                     /* 地址高字节 */
    bb_out_byte(0x39u);                     /* 地址低字节 = VERSIONR */
    bb_out_byte(0x80u);                     /* RWB=1 读 | BSB=0 common */

    for (i = 0u; i < 8u; i++)
    {
        v = (uint8_t)((v << 1) | bb_in_bit());
    }

    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_SET);
    bb_pin_mode(0u);                        /* 还给 SPI2 */

    return v;
}

uint8_t w5500_init(void)
{
    GPIO_InitTypeDef g = {0};
    uint32_t t0;

    /* 片选脚自己配（CubeMX 里没有这个脚）：先把 A 口时钟确认开着 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = W5500_CS_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(W5500_CS_PORT, &g);
    w_cs(0u);                   /* 空闲拉高：共用总线，不能一直选中它 */

    /* 软复位：MR 的 RST 位置 1，芯片复位完自己清掉 */
    w5500_write8(W5500_MR, W5500_BSB_COMMON, 0x80u);
    t0 = HAL_GetTick();
    while ((w5500_read8(W5500_MR, W5500_BSB_COMMON) & 0x80u) != 0u)
    {
        if ((HAL_GetTick() - t0) > 100u)        /* 有界等待，不无限阻塞 */
        {
            s_err++;
            return 1u;
        }
    }

    s_ver = w5500_read8(W5500_VERSIONR, W5500_BSB_COMMON);
    return (s_ver == 0x04u) ? 0u : 1u;          /* 手册写死的固定值 */
}
