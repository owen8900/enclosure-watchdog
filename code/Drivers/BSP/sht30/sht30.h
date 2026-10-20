/**
  ******************************************************************************
  * 文件    : sht30.h
  * 功能    : SHT30 温湿度传感器接口（挂 I2C2，收发走 i2c_bus 的带超时原语）
  ******************************************************************************
  */

#ifndef __SHT30_H
#define __SHT30_H

#include "main.h"
#include <stdint.h>

#define SHT30_ADDR_1    0x44u       /* ADDR 脚接 GND */
#define SHT30_ADDR_2    0x45u       /* ADDR 脚接 VDD */

typedef struct
{
    int32_t temp_milli;             /* 温度，千分之一摄氏度 */
    int32_t rh_milli;               /* 湿度，千分之一 %RH */
} sht30_data_t;

uint8_t sht30_probe(uint8_t addr);                      /* 0 = 总线上有应答 */
uint8_t sht30_read(uint8_t addr, sht30_data_t *d);       /* 0 = 读成功，非 0 见 .c 的说明 */
void    sht30_selftest(void);                            /* 开机把两个地址各读一次 */
void    sht30_poll(void);                                /* 主循环周期调用 */

#endif /* __SHT30_H */
