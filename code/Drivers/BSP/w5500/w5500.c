/**
  ******************************************************************************
  * 文件    : w5500.c
  * 功能    : W5500 以太网控制器（SPI）驱动实现 + socket0 上的 UDP 收发
  * 说明    : 地址相位 3 字节 = addr[15:8] / addr[7:0] / (BSB<<3 | RWB)，读 RWB=0
  * 约定    : 每次传输都带超时（100ms），失败计数，不无限阻塞
  ******************************************************************************
  */

#include "w5500.h"
#include "spi.h"
#include "xpt2046.h"
#include "bsp_log.h"
#include <string.h>

/* Plan A 的硬前提：PB14 得是 SPI2_MISO，而它原来被触摸时钟占着。
 * 条件不满足就编译失败，免得编出一个 MISO 被触摸时钟占着的固件去插网线。 */
#if !defined(XPT_CLK_ON_PC2) || (XPT_CLK_ON_PC2 != 1)
  #error "W5500 needs PB14 as SPI2_MISO: set XPT_CLK_ON_PC2 to 1 (touch clock moves to PC2)"
#endif

#define W5500_SPI_TIMEOUT   100u

static uint32_t s_err;
static uint32_t s_rxerr;   /* 收包时读失败的次数，watch 行里看 */
static uint8_t  s_ver;
static uint8_t  s_tx[8];      /* 探针用：发 0xFF 看 MISO */

/* 片选，低有效。与屏共用 SPI2 时，同时只有被选中的那个设备会理会总线 */
static void w_cs(uint8_t assert)
{
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN,
                      assert ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* 发地址相位 3 字节。控制字节 = BSB[7:3] | RWB[2]：读 0x00、写 0x04。
 * 这一点踩过坑：原来按读位 0x80 发，等于每次读都在对错误块发写命令，
 * 芯片从不驱动 MISO，VERSIONR 一直读回 0x00。 */
static uint8_t w_addr(uint16_t addr, uint8_t bsb, uint8_t read_op)
{
    uint8_t h[3];

    h[0] = (uint8_t)(addr >> 8);
    h[1] = (uint8_t)(addr & 0xFFu);
    h[2] = (uint8_t)((uint8_t)(bsb << 3) | (read_op ? 0x00u : 0x04u));

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

/* 总线探针：发一个"真正的 VERSIONR 读帧"（0x00 0x39 0x00，后面补 0x00 把数据移出来），
 * 把 MISO 上看到的字节原样带出去。判据唯一：out[3] == 0x04 就是芯片正常应答。
 * 前 3 字节本来就是 0x00 —— 地址相位期间从机不驱动 MISO，那三个 0 说明不了问题。 */
void w5500_bus_probe(uint8_t *out, uint8_t n)
{
    uint8_t i;

    if (n > sizeof(s_tx))
    {
        n = (uint8_t)sizeof(s_tx);
    }
    for (i = 0; i < n; i++)
    {
        s_tx[i] = 0x00u;
    }
    if (n >= 4u)
    {
        s_tx[1] = 0x39u;                    /* 地址低字节 = VERSIONR */
        s_tx[2] = 0x00u;                    /* BSB=0 common | 读(0x00) */
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
 * 读 MISO 时开内部上拉 —— 芯片不驱动时读成 1（跟被拉低分开）。
 * --------------------------------------------------------------- */
static void bb_delay(void)
{
    volatile uint32_t i;

    for (i = 0u; i < 700u; i++)     /* 半周期约几微秒；W5500 不挑速度 */
    {
    }
}

/* 切到位翻转要的方向：SCK/MOSI 输出，MISO 输入带上拉 */
static void bb_pins_io(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin = GPIO_PIN_13 | GPIO_PIN_15;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    g.Pin = GPIO_PIN_14;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);
}

/* 三个脚还给 SPI2（AF5），别把总线留在位翻转状态 */
static void bb_pins_af(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &g);
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

uint8_t w5500_bitbang_read8(uint16_t addr, uint8_t bsb)
{
    uint8_t v = 0u;
    uint8_t i;

    bb_pins_io();
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_RESET);
    bb_delay();

    bb_out_byte((uint8_t)(addr >> 8));                      /* 地址高字节 */
    bb_out_byte((uint8_t)(addr & 0xFFu));                   /* 地址低字节 */
    bb_out_byte((uint8_t)((uint8_t)(bsb << 3) | 0x00u));    /* BSB | 读 */

    for (i = 0u; i < 8u; i++)
    {
        v = (uint8_t)((v << 1) | bb_in_bit());
    }

    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_SET);
    bb_pins_af();

    return v;
}

/* ---------------------------------------------------------------
 * socket0 上的 UDP：静态 IP + 收发
 * 命令与指针操作照官方 ioLibrary 的顺序：先等缓存空间，再写指针和数据，
 * 发 SEND/RECV 命令后必须等 Sn_CR 被芯片清零。
 * --------------------------------------------------------------- */

/* socket 的寄存器块选择位：REG = 1+4n、TX = 2+4n、RX = 3+4n（官方 w5500.h 的编码） */
static uint8_t w5500_bsb_reg(uint8_t s) { return (uint8_t)(0x01u + 4u * s); }
static uint8_t w5500_bsb_tx(uint8_t s)  { return (uint8_t)(0x02u + 4u * s); }
static uint8_t w5500_bsb_rx(uint8_t s)  { return (uint8_t)(0x03u + 4u * s); }

/* socket 里的 16 位寄存器是高字节在前（大端），别按小端读 */
static uint16_t sn_read16(uint8_t s, uint16_t reg)
{
    uint8_t b[2];

    if (w5500_read(reg, w5500_bsb_reg(s), b, 2u) != 0u)
    {
        return 0u;
    }
    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

static void sn_write16(uint8_t s, uint16_t reg, uint16_t val)
{
    uint8_t b[2];

    b[0] = (uint8_t)(val >> 8);
    b[1] = (uint8_t)(val & 0xFFu);
    (void)w5500_write(reg, w5500_bsb_reg(s), b, 2u);
}

/* 写 Sn_CR 后等芯片把它清零：清完才代表命令被接收 */
static uint8_t sn_cmd(uint8_t s, uint8_t cr)
{
    uint32_t t0 = HAL_GetTick();

    w5500_write8(W5500_SN_CR, w5500_bsb_reg(s), cr);
    while (w5500_read8(W5500_SN_CR, w5500_bsb_reg(s)) != 0u)
    {
        if ((HAL_GetTick() - t0) > 100u)
        {
            s_err++;
            return 1u;
        }
    }
    return 0u;
}

/* 配 MAC/IP/网关/掩码，写完读回比一遍 —— 写没进去在启动日志里就能看出来 */
uint8_t w5500_net_setup(void)
{
    static const uint8_t mac[6]  = W5500_CFG_MAC;
    static const uint8_t ip[4]   = W5500_CFG_IP;
    static const uint8_t gw[4]   = W5500_CFG_GW;
    static const uint8_t mask[4] = W5500_CFG_MASK;
    uint8_t rd[6];

    if (w5500_write(W5500_SHAR, W5500_BSB_COMMON, mac, 6u) != 0u)  return 1u;
    if (w5500_write(W5500_SIPR, W5500_BSB_COMMON, ip, 4u) != 0u)   return 1u;
    if (w5500_write(W5500_GAR, W5500_BSB_COMMON, gw, 4u) != 0u)    return 1u;
    if (w5500_write(W5500_SUBR, W5500_BSB_COMMON, mask, 4u) != 0u) return 1u;

    if (w5500_read(W5500_SIPR, W5500_BSB_COMMON, rd, 4u) != 0u)    return 2u;
    if (memcmp(rd, ip, 4u) != 0)                                   return 2u;
    if (w5500_read(W5500_SHAR, W5500_BSB_COMMON, rd, 6u) != 0u)    return 3u;
    if (memcmp(rd, mac, 6u) != 0)                                  return 3u;

    return 0u;
}

uint8_t w5500_socket_sr(uint8_t s)
{
    return w5500_read8(W5500_SN_SR, w5500_bsb_reg(s));
}

/* 顺序照手册：先 CLOSE 清旧状态，再写模式/端口/缓存，最后 OPEN，看 Sn_SR 对不对 */
uint8_t w5500_udp_open(uint8_t s, uint16_t port)
{
    uint8_t bsb = w5500_bsb_reg(s);
    uint8_t b[2];

    if (s > 7u)
    {
        return 1u;
    }

    (void)sn_cmd(s, W5500_SN_CR_CLOSE);
    w5500_write8(W5500_SN_IR, bsb, 0xFFu);          /* 清掉上一轮的残留中断 */

    w5500_write8(W5500_SN_MR, bsb, W5500_SN_MR_UDP);
    b[0] = (uint8_t)(port >> 8);
    b[1] = (uint8_t)(port & 0xFFu);
    (void)w5500_write(W5500_SN_PORT, bsb, b, 2u);
    w5500_write8(W5500_SN_RXBUF_SIZE, bsb, 2u);     /* 2KB，默认值，显式写一遍 */
    w5500_write8(W5500_SN_TXBUF_SIZE, bsb, 2u);

    if (sn_cmd(s, W5500_SN_CR_OPEN) != 0u)
    {
        return 2u;
    }
    return (w5500_socket_sr(s) == W5500_SN_SR_UDP) ? 0u : 3u;
}

/* UDP 发送。先看目的 IP 和端口，再确认缓存够大；ARP 不通时芯片会等
 * RTR*RCR 约 1.8 秒才置 TIMEOUT，所以这里给 3 秒就够判"发不出去"。 */
uint8_t w5500_udp_send(uint8_t s, const uint8_t dip[4], uint16_t dport,
                       const uint8_t *buf, uint16_t len)
{
    uint8_t  bsb_reg = w5500_bsb_reg(s);
    uint16_t wr;
    uint32_t t0;
    uint8_t  ir;

    if (len == 0u || len > W5500_BUF_SIZE)
    {
        return 1u;
    }
    if (w5500_socket_sr(s) != W5500_SN_SR_UDP)
    {
        return 2u;
    }

    t0 = HAL_GetTick();
    while (sn_read16(s, W5500_SN_TX_FSR) < len)
    {
        if ((HAL_GetTick() - t0) > 5000u)
        {
            s_err++;
            return 3u;                  /* 上一包还没发出去 */
        }
        HAL_Delay(1u);
    }

    if (w5500_write(W5500_SN_DIPR, bsb_reg, dip, 4u) != 0u)
    {
        return 4u;
    }
    sn_write16(s, W5500_SN_DPORT, dport);

    wr = sn_read16(s, W5500_SN_TX_WR);
    if (w5500_write(wr, w5500_bsb_tx(s), buf, len) != 0u)
    {
        return 5u;
    }
    sn_write16(s, W5500_SN_TX_WR, (uint16_t)(wr + len));    /* 16 位指针自己回绕 */

    if (sn_cmd(s, W5500_SN_CR_SEND) != 0u)
    {
        return 6u;
    }

    t0 = HAL_GetTick();
    for (;;)
    {
        ir = w5500_read8(W5500_SN_IR, bsb_reg);
        if ((ir & W5500_SN_IR_SEND_OK) != 0u)
        {
            w5500_write8(W5500_SN_IR, bsb_reg, W5500_SN_IR_SEND_OK);
            return 0u;
        }
        if ((ir & W5500_SN_IR_TIMEOUT) != 0u)
        {
            w5500_write8(W5500_SN_IR, bsb_reg, W5500_SN_IR_TIMEOUT);
            return 7u;                  /* ARP 没应答：目的 IP 不通或网段不对 */
        }
        if ((HAL_GetTick() - t0) > 3000u)
        {
            return 8u;
        }
        HAL_Delay(1u);
    }
}

/* 收一包。UDP 模式下缓存最前面 8 字节是来源信息，读完要把 Sn_RX_RD 推到包尾
 * 再发 RECV，少了这一步同一包会被反复读出来。 */
uint16_t w5500_udp_recv(uint8_t s, uint8_t *buf, uint16_t max,
                        uint8_t from_ip[4], uint16_t *from_port)
{
    uint8_t  head[W5500_UDP_INFO_LEN];
    uint16_t rd;
    uint16_t total;
    uint16_t n;

    if (sn_read16(s, W5500_SN_RX_RSR) < W5500_UDP_INFO_LEN)
    {
        return 0u;
    }

    rd = sn_read16(s, W5500_SN_RX_RD);
    if (w5500_read(rd, w5500_bsb_rx(s), head, W5500_UDP_INFO_LEN) != 0u)
    {
        s_rxerr++;
        return 0u;
    }

    if (from_ip != 0)
    {
        from_ip[0] = head[0];
        from_ip[1] = head[1];
        from_ip[2] = head[2];
        from_ip[3] = head[3];
    }
    if (from_port != 0)
    {
        *from_port = (uint16_t)(((uint16_t)head[4] << 8) | (uint16_t)head[5]);
    }

    total = (uint16_t)(((uint16_t)head[6] << 8) | (uint16_t)head[7]);  /* 包长来自头部，不是 RSR */
    n = total;
    if (n > max)
    {
        n = max;
    }
    if (n > W5500_BUF_SIZE)
    {
        n = W5500_BUF_SIZE;
    }
    if (n > 0u)
    {
        if (w5500_read((uint16_t)(rd + W5500_UDP_INFO_LEN), w5500_bsb_rx(s), buf, n) != 0u)
        {
            s_rxerr++;
            return 0u;
        }
    }

    /* 读指针推到包尾再发 RECV，不然同一包会被反复读出来；16 位指针自己回绕 */
    sn_write16(s, W5500_SN_RX_RD, (uint16_t)(rd + W5500_UDP_INFO_LEN + total));
    if (sn_cmd(s, W5500_SN_CR_RECV) != 0u)
    {
        return 0u;
    }

    return n;
}

/* 日志里打数字：串口打印只走 ASCII，不引 printf */
/* 拼 "CG UDP heartbeat N\r\n"，不用 printf */
static uint16_t build_heartbeat(uint8_t *buf, uint16_t seq)
{
    static const char head[] = "CG UDP heartbeat ";
    char    num[6];
    uint16_t i = 0u;
    uint16_t j;
    uint16_t k = 0u;

    for (j = 0u; head[j] != 0; j++)
    {
        buf[i++] = (uint8_t)head[j];
    }
    while (seq > 0u && k < 5u)
    {
        num[k++] = (char)('0' + (char)(seq % 10u));
        seq /= 10u;
    }
    if (k == 0u)
    {
        num[k++] = '0';
    }
    while (k > 0u)
    {
        buf[i++] = (uint8_t)num[--k];
    }
    buf[i++] = (uint8_t)'\r';
    buf[i++] = (uint8_t)'\n';

    return i;
}

/* 网络起来：配静态 IP + 开 socket0 UDP。0 = 全好，1 = 配网失败，2 = socket 没开起来 */
uint8_t w5500_net_up(void)
{
    uint8_t rc = w5500_net_setup();

    if (rc != 0u)
    {
        bsp_log("net cfg ERR ");
        bsp_log_u32((uint32_t)rc);        /* 1=写失败 2=IP 读回不符 3=MAC 读回不符 */
        bsp_log("\r\n");
        return 1u;
    }
    bsp_log("net cfg OK\r\n");
    bsp_log((w5500_link_up() != 0u) ? "link up\r\n" : "link down, check cable\r\n");

    rc = w5500_udp_open(0u, W5500_CFG_LOCAL_PORT);
    if (rc != 0u)
    {
        bsp_log("udp open ERR ");
        bsp_log_u32((uint32_t)rc);        /* 3 = Sn_SR 不是 0x22 */
        bsp_log("\r\n");
        return 2u;
    }
    bsp_log("udp open OK  port 5000\r\n");

    return 0u;
}

/* 收到的内容可能是任意字节，非可打印的打成点，免得串口乱码 */
/* 收一包：打印来源和内容，再原样回一个。没数据立刻返回，主循环里随便调 */
void w5500_udp_poll(void)
{
    static uint8_t rx[160];
    uint8_t  from[4];
    uint16_t from_port = 0u;
    uint16_t n;

    n = w5500_udp_recv(0u, rx, (uint16_t)sizeof(rx), from, &from_port);
    if (n == 0u)
    {
        return;
    }

    bsp_log("udp recv ");
    bsp_log_u32((uint32_t)n);
    bsp_log(" B from ");
    bsp_log_u32((uint32_t)from[0]);
    bsp_log(".");
    bsp_log_u32((uint32_t)from[1]);
    bsp_log(".");
    bsp_log_u32((uint32_t)from[2]);
    bsp_log(".");
    bsp_log_u32((uint32_t)from[3]);
    bsp_log(":");
    bsp_log_u32((uint32_t)from_port);
    bsp_log("  [");
    bsp_log_ascii(rx, n);
    bsp_log("]\r\n");

    (void)w5500_udp_send(0u, from, from_port, rx, n);   /* 原样回一个，PC 上能看到闭环 */
}

/* 心跳：每秒一包。只在"发失败"和"恢复"这两个转折点打印，免得串口被刷屏 */
void w5500_udp_send_heartbeat(void)
{
    static const uint8_t pc[4] = W5500_CFG_PC_IP;
    static uint8_t  tx[40];
    static uint16_t seq = 0u;
    static uint8_t  failing = 0u;
    uint16_t len;
    uint8_t  rc;

    seq++;
    len = build_heartbeat(tx, seq);
    rc = w5500_udp_send(0u, pc, W5500_CFG_PC_PORT, tx, len);

    if (rc != 0u)
    {
        if (failing == 0u)
        {
            bsp_log("udp send ERR ");
            bsp_log_u32((uint32_t)rc);    /* 7 = ARP 超时：PC 的 IP 或网段不对 */
            bsp_log("\r\n");
        }
        failing = 1u;
    }
    else if (failing != 0u)
    {
        failing = 0u;
        bsp_log("udp send OK again\r\n");
    }
}

/* 主循环里的诊断行：把读 VERSIONR 的原始帧带出来，再加 RTR（应为 07D0）和 socket0 状态
 * （应为 22）对账 —— 读回 0x00 时就能分清"芯片没应答"和"芯片真的回了 0"。 */
void w5500_watch_dump(void)
{
    uint8_t  p[4];
    uint8_t  rb[2];
    uint8_t  i;
    uint16_t rtr;

    w5500_bus_probe(p, 4u);
    (void)w5500_read(W5500_RTR, W5500_BSB_COMMON, rb, 2u);
    rtr = (uint16_t)(((uint16_t)rb[0] << 8) | (uint16_t)rb[1]);

    bsp_log("W5500 watch: frame=");
    for (i = 0u; i < 4u; i++)
    {
        bsp_log_hex2(p[i]);
        bsp_log(" ");
    }
    bsp_log(" VERSIONR=0x");
    bsp_log_hex2(w5500_read8(W5500_VERSIONR, W5500_BSB_COMMON));
    bsp_log(" RTR=0x");
    bsp_log_hex2((uint8_t)(rtr >> 8));
    bsp_log_hex2((uint8_t)(rtr & 0xFFu));
    bsp_log(" sr0=0x");
    bsp_log_hex2(w5500_socket_sr(0u));
    bsp_log(" rsr=0x");
    {
        uint16_t rsr = sn_read16(0u, W5500_SN_RX_RSR);

        bsp_log_hex2((uint8_t)(rsr >> 8));
        bsp_log_hex2((uint8_t)(rsr & 0xFFu));
    }
    bsp_log(" rxerr=");
    bsp_log_u32(s_rxerr);
    bsp_log(" err=");
    bsp_log_u32(w5500_error_count());
    bsp_log("\r\n");
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
