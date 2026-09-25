/**
  ******************************************************************************
  * 文件    : ds18b20.h
  * 功能    : DS18B20 单总线温度传感器（1-Wire 位翻转，DQ = PC13）
  * 硬件    : 板载单总线座（丝印 DS18B20/DHT11，V1.5 位号 U4），R22 4.7K 上拉已焊
  *           也可以不用座、直接三根线：VCC→3.3V、GND→GND、DQ→左侧排针 PC13
  * 说明    : PC13 由本驱动在运行时配成开漏输出，CubeMX 里配没配都不影响
  ******************************************************************************
  */

#ifndef __DS18B20_H
#define __DS18B20_H

#include "main.h"
#include <stdint.h>

#define DS18B20_DQ_PORT     GPIOC
#define DS18B20_DQ_PIN      GPIO_PIN_13

uint8_t ds18b20_init(void);                            /* 0 = 复位脉冲后有器件应答 */
uint8_t ds18b20_read_rom(uint8_t rom[8]);              /* 0 = 读到 8 字节 ROM 且 CRC 对 */
uint8_t ds18b20_read_temp(int16_t *raw);               /* 0 = 读到 12 位原始值（1/16 °C） */
int32_t ds18b20_raw_to_milli(int16_t raw);             /* 原始值 -> 千分之一摄氏度 */
void    ds18b20_selftest(void);                        /* 开机自检：器件应答 + ROM + 一次读数 */
void    ds18b20_poll(void);                            /* 主循环周期调用：读一次并打印 */

#endif /* __DS18B20_H */
