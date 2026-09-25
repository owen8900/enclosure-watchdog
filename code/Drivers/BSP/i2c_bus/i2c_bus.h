/**
  ******************************************************************************
  * 文件    : i2c_bus.h
  * 功能    : I2C2 总线工具（扫描 + 收发原语），给 SHT30 / INA219 共用
  * 硬件    : PB10 = I2C2_SCL、PB11 = I2C2_SDA，都在 P2 座上（丝印 SCL/SDA）
  *           板上已有 4.7K 上拉（R23/R25），模块自带的上拉并联后仍在允许范围
  ******************************************************************************
  */

#ifndef __I2C_BUS_H
#define __I2C_BUS_H

#include "main.h"
#include <stdint.h>

#define I2C_BUS_SCAN_FIRST  0x08u       /* 7 位地址扫描范围（跳过保留地址） */
#define I2C_BUS_SCAN_LAST   0x77u
#define I2C_BUS_SCAN_MAX    16u

void    i2c_bus_set_log(void (*log)(const char *s));   /* 传 uart_puts 即可 */
uint8_t i2c_bus_scan(uint8_t *found, uint8_t max);     /* 返回在线器件个数 */
void    i2c_bus_selftest(void);                        /* 开机扫一遍并打印 */
void    i2c_bus_poll(void);                            /* 主循环周期调用：结果变了才打印 */
uint8_t i2c_bus_error_count(void);

uint8_t i2c_bus_write(uint8_t addr, const uint8_t *buf, uint16_t len);
uint8_t i2c_bus_read(uint8_t addr, uint8_t *buf, uint16_t len);
uint8_t i2c_bus_mem_write(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len);
uint8_t i2c_bus_mem_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

#endif /* __I2C_BUS_H */
