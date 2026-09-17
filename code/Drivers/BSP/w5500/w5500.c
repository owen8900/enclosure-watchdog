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

/* PHYCFGR 的 bit0 = 链路状态：插上网线、对端也在跑时读 1 */
uint8_t w5500_link_up(void)
{
    return (uint8_t)(w5500_read8(W5500_PHYCFGR, W5500_BSB_COMMON) & 0x01u);
}

uint32_t w5500_error_count(void)
{
    return s_err;
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
