/**
  ******************************************************************************
  * 文件    : w5500.h
  * 功能    : W5500 以太网控制器（SPI）驱动接口
  * 硬件    : 与屏共用 SPI2：SCK=PB13  MOSI=PB15  MISO=PB14
  *           W5500 侧：SCS=PA8（低有效） RST 接 3.3V（不控） INT 不接（用轮询）
  *           ⚠️ 共用 SPI2 的前提：xpt2046.h 的 XPT_CLK_ON_PC2 必须为 1
  *              （触摸时钟让到 PC2，PB14 才能当 SPI2_MISO）
  ******************************************************************************
  */

#ifndef __W5500_H
#define __W5500_H

#include "main.h"
#include <stdint.h>

/* 片选：PA8 在 CubeMX 里没有配，由 w5500_init() 自己配成推挽输出
 * （运行时配置，重新 GENERATE CODE 也冲不掉）。
 * PA8 带着红外接收头的 4.7K 上拉，原厂说明可用作输出 —— 这里只做输出。 */
#define W5500_CS_PORT       GPIOA
#define W5500_CS_PIN        GPIO_PIN_8

/* 块选择位。控制字节 = BSB[7:3] | RWB[2] | 模式[1:0]，所以这里存的是
 * 未移位的 BSB 值，发帧时左移 3 位（见 w_addr()）。
 * 编码规则出自官方 WIZnet ioLibrary_Driver/Ethernet/W5500/w5500.h：
 *   WIZCHIP_CREG_BLOCK 0x00 / WIZCHIP_SREG_BLOCK(N) (1+4*N) / TXBUF (2+4*N) / RXBUF (3+4*N) */
#define W5500_BSB_COMMON    0x00u
#define W5500_BSB_S0_REG    0x01u       /* 1 + 4*0 */
#define W5500_BSB_S0_TX     0x02u       /* 2 + 4*0 */
#define W5500_BSB_S0_RX     0x03u       /* 3 + 4*0 */

/* 常用寄存器（common 块） */
#define W5500_MR            0x0000u     /* 模式：bit7 = 软复位 */
#define W5500_GAR           0x0001u     /* 网关地址 */
#define W5500_SUBR          0x0005u     /* 子网掩码 */
#define W5500_SHAR          0x0009u     /* 本机 MAC */
#define W5500_SIPR          0x000Fu     /* 本机 IP */
#define W5500_RTR           0x0019u     /* 重试超时 */
#define W5500_RCR           0x001Bu     /* 重试次数 */
#define W5500_PHYCFGR       0x002Eu     /* PHY 配置/状态，bit0 = 链路 */
#define W5500_VERSIONR      0x0039u     /* 版本寄存器，固定读 0x04 */

/* Socket 寄存器：地址是 socket 块内的偏移，块选择位随 socket 号变（见 w5500_bsb_reg） */
#define W5500_SN_MR         0x0000u     /* 协议：0x02 = UDP */
#define W5500_SN_CR         0x0001u     /* 命令：写完芯片自己清零 */
#define W5500_SN_IR         0x0002u     /* 中断标志：写 1 清零 */
#define W5500_SN_SR         0x0003u     /* 状态：0x22 = UDP 已开 */
#define W5500_SN_PORT       0x0004u     /* 本端端口，必须在 OPEN 之前写 */
#define W5500_SN_DIPR       0x000Cu     /* 目的 IP（SEND 用） */
#define W5500_SN_DPORT      0x0010u     /* 目的端口（SEND 用） */
#define W5500_SN_RXBUF_SIZE 0x001Eu     /* 接收缓存 K 字节，默认 2 */
#define W5500_SN_TXBUF_SIZE 0x001Fu     /* 发送缓存 K 字节，默认 2 */
#define W5500_SN_TX_FSR     0x0020u     /* 发送缓存剩余空间，读之前先判它 */
#define W5500_SN_TX_WR      0x0024u     /* 发送写指针，写完数据要自己加 len */
#define W5500_SN_RX_RSR     0x0026u     /* 已收到的字节数，0 = 没数据 */
#define W5500_SN_RX_RD      0x0028u     /* 接收读指针，读完一包要推到包尾 */

#define W5500_SN_MR_UDP     0x02u
#define W5500_SN_CR_OPEN    0x01u
#define W5500_SN_CR_CLOSE   0x10u
#define W5500_SN_CR_SEND    0x20u
#define W5500_SN_CR_RECV    0x40u
#define W5500_SN_IR_SEND_OK 0x10u
#define W5500_SN_IR_TIMEOUT 0x08u
#define W5500_SN_IR_RECV    0x04u
#define W5500_SN_SR_CLOSED  0x00u
#define W5500_SN_SR_UDP     0x22u

/* 每个 socket 默认分到 2KB，8 个正好占满 W5500 内部 16KB，这里不改分配 */
#define W5500_BUF_SIZE      2048u

/* UDP 收包时接收缓存的前 8 字节是来源信息（IP 4 + 端口 2 + 长度 2），不是数据 */
#define W5500_UDP_INFO_LEN  8u

/* M9 自检用的网络参数。换成你 PC 所在网段就行，改完重编重烧。
 * MAC 第一个字节必须是偶数；PC 的 IP 填有线网口那个（直连时自己设的静态 IP） */
#define W5500_NET_TEST        1        /* 1 = 开机配静态 IP 并开 UDP；不想联网改 0 */
#define W5500_CFG_MAC         {0x0Cu, 0x29u, 0xABu, 0x7Cu, 0x00u, 0x01u}
#define W5500_CFG_IP          {192u, 168u, 1u, 199u}
#define W5500_CFG_GW          {192u, 168u, 1u, 1u}
#define W5500_CFG_MASK        {255u, 255u, 255u, 0u}
#define W5500_CFG_PC_IP       {192u, 168u, 1u, 100u}
#define W5500_CFG_LOCAL_PORT  5000u    /* 模块自己的 UDP 端口 */
#define W5500_CFG_PC_PORT     6000u    /* PC 上 NetAssist 的本机端口 */
#define W5500_UDP_HB_MS       1000u    /* 主循环里心跳包的发送周期 */

uint8_t  w5500_init(void);              /* 0 = 成功（软复位后 VERSIONR == 0x04） */
uint8_t  w5500_version(void);
uint8_t  w5500_link_up(void);
uint32_t w5500_error_count(void);

uint8_t  w5500_read(uint16_t addr, uint8_t bsb, uint8_t *buf, uint16_t len);
void     w5500_bus_probe(uint8_t *out, uint8_t n);   /* 诊断：读 VERSIONR，带地址相位 */
uint8_t  w5500_miso_idle(uint8_t *fast);              /* 诊断：MISO 开上拉后的静态电平 */
uint8_t  w5500_bitbang_read8(uint16_t addr, uint8_t bsb);  /* 诊断：位翻转慢速 SPI 读任意寄存器 */
uint8_t  w5500_write(uint16_t addr, uint8_t bsb, const uint8_t *buf, uint16_t len);
uint8_t  w5500_read8(uint16_t addr, uint8_t bsb);
void     w5500_write8(uint16_t addr, uint8_t bsb, uint8_t val);

void     w5500_set_log(void (*log)(const char *s));   /* 自检日志，传 uart_puts 即可 */
uint8_t  w5500_net_setup(void);                        /* 0 = 写参数并读回校验通过 */
uint8_t  w5500_udp_open(uint8_t s, uint16_t port);     /* 0 = 开好后 Sn_SR 是 UDP */
uint8_t  w5500_udp_send(uint8_t s, const uint8_t dip[4], uint16_t dport,
                        const uint8_t *buf, uint16_t len);
uint16_t w5500_udp_recv(uint8_t s, uint8_t *buf, uint16_t max,
                        uint8_t from_ip[4], uint16_t *from_port);
uint8_t  w5500_socket_sr(uint8_t s);
void     w5500_watch_dump(void);                       /* 诊断：原始读帧 + RTR + socket 状态 */
uint8_t  w5500_net_up(void);                           /* 配网 + 开 UDP：0 = 全好 */
void     w5500_udp_poll(void);                         /* 收包并回显，没数据立刻返回 */
void     w5500_udp_send_heartbeat(void);               /* 发一包心跳到 PC */

#endif /* __W5500_H */
