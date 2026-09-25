/**
  ******************************************************************************
  * 文件    : bsp_log.h
  * 功能    : BSP 层共用的串口日志出口（数字/十六进制/定点小数的打印）
  * 说明    : 开机在 main 里 bsp_log_set(uart_puts) 一次，其它模块直接调
  *           bsp_log_*；没设过出口就静默丢弃，不会崩
  * 约定    : 出去的字符串一律 ASCII（XCOM 按 GBK 解码，中文会乱码）
  ******************************************************************************
  */

#ifndef __BSP_LOG_H
#define __BSP_LOG_H

#include "main.h"
#include <stdint.h>

void bsp_log_set(void (*log)(const char *s));    /* 传 uart_puts 即可 */
void bsp_log(const char *s);

void bsp_log_u32(uint32_t v);                    /* 十进制整数 */
void bsp_log_hex2(uint8_t v);                    /* 两位十六进制 */
void bsp_log_milli(int32_t milli);               /* 千分之一 → 25.625 */
void bsp_log_ascii(const uint8_t *buf, uint16_t n);  /* 不可打印字节打成点 */

#endif /* __BSP_LOG_H */
