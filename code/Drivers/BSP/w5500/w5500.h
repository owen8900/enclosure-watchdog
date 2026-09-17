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

/* 块选择位（地址相位第 3 字节的低 5 位） */
#define W5500_BSB_COMMON    0x00u
#define W5500_BSB_S0_REG    0x01u
#define W5500_BSB_S0_TX     0x09u
#define W5500_BSB_S0_RX     0x11u

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

uint8_t  w5500_init(void);              /* 0 = 成功（软复位后 VERSIONR == 0x04） */
uint8_t  w5500_version(void);
uint8_t  w5500_link_up(void);
uint32_t w5500_error_count(void);

uint8_t  w5500_read(uint16_t addr, uint8_t bsb, uint8_t *buf, uint16_t len);
void     w5500_bus_probe(uint8_t *out, uint8_t n);   /* 诊断：看 MISO 有没有反应 */
uint8_t  w5500_miso_idle(void);                      /* 诊断：MISO 开上拉读到的静态电平 */
uint8_t  w5500_write(uint16_t addr, uint8_t bsb, const uint8_t *buf, uint16_t len);
uint8_t  w5500_read8(uint16_t addr, uint8_t bsb);
void     w5500_write8(uint16_t addr, uint8_t bsb, uint8_t val);

#endif /* __W5500_H */
