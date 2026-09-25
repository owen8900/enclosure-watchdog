/**
  ******************************************************************************
  * 文件    : i2c_bus.c
  * 功能    : I2C2 总线工具实现（扫描 + 收发原语）
  * 说明    : 扫描用 HAL_I2C_IsDeviceReady 逐地址探测，每次探测都有超时，
  *           所以空总线也能在很短时间内扫完 0x08~0x77，不会卡住
  * 约定    : 每次传输都带超时（100ms），失败计数，不无限阻塞
  ******************************************************************************
  */

#include "i2c_bus.h"
#include "i2c.h"
#include "bsp_log.h"

#define I2C_BUS_TIMEOUT     100u        /* 单次收发超时（ms） */
#define I2C_BUS_PROBE_MS    5u          /* 扫描时单次探测超时（ms） */

static uint32_t s_err;
static uint8_t  s_last[I2C_BUS_SCAN_MAX];
static uint8_t  s_last_n = 0xFFu;       /* 0xFF = 还没扫过，第一次一定打印 */

uint8_t i2c_bus_error_count(void)
{
    return (uint8_t)s_err;
}

uint8_t i2c_bus_scan(uint8_t *found, uint8_t max)
{
    uint8_t n = 0u;
    uint8_t a;

    for (a = I2C_BUS_SCAN_FIRST; a <= I2C_BUS_SCAN_LAST; a++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)((uint16_t)a << 1), 1u,
                                  I2C_BUS_PROBE_MS) == HAL_OK)
        {
            if (n >= max)
            {
                break;
            }
            found[n] = a;
            n++;
        }
    }

    return n;
}

static void log_found(const char *tag, const uint8_t *found, uint8_t n)
{
    uint8_t i;

    bsp_log(tag);
    if (n == 0u)
    {
        bsp_log("nothing found\r\n");
        return;
    }
    bsp_log_u32((uint32_t)n);
    bsp_log(" device(s):");
    for (i = 0u; i < n; i++)
    {
        bsp_log(" 0x");
        bsp_log_hex2(found[i]);
    }
    bsp_log("\r\n");
}

void i2c_bus_selftest(void)
{
    uint8_t found[I2C_BUS_SCAN_MAX];
    uint8_t n;
    uint8_t i;

    bsp_log("I2C2 bus   : scan 0x08..0x77 -> ");
    n = i2c_bus_scan(found, I2C_BUS_SCAN_MAX);
    log_found("", found, n);

    if (n == 0u)
    {
        bsp_log("I2C2 check : SCL=PB10 SDA=PB11 3V3 GND (P2 header), board has 4.7K pull-ups\r\n");
    }

    for (i = 0u; i < I2C_BUS_SCAN_MAX; i++)
    {
        s_last[i] = (i < n) ? found[i] : 0u;
    }
    s_last_n = n;
}

/* 主循环里周期调用：总线上的器件增删时才打印，插一只报一只 */
void i2c_bus_poll(void)
{
    uint8_t found[I2C_BUS_SCAN_MAX];
    uint8_t n;
    uint8_t i;
    uint8_t same = 1u;

    n = i2c_bus_scan(found, I2C_BUS_SCAN_MAX);

    if (n != s_last_n)
    {
        same = 0u;
    }
    else
    {
        for (i = 0u; i < n; i++)
        {
            if (found[i] != s_last[i])
            {
                same = 0u;
                break;
            }
        }
    }
    if (same != 0u)
    {
        return;
    }

    for (i = 0u; i < I2C_BUS_SCAN_MAX; i++)
    {
        s_last[i] = (i < n) ? found[i] : 0u;
    }
    s_last_n = n;

    bsp_log("I2C2 bus   : changed -> ");
    log_found("", found, n);
}

uint8_t i2c_bus_write(uint8_t addr, const uint8_t *buf, uint16_t len)
{
    if (HAL_I2C_Master_Transmit(&hi2c2, (uint16_t)((uint16_t)addr << 1),
                                (uint8_t *)buf, (uint16_t)len, I2C_BUS_TIMEOUT) != HAL_OK)
    {
        s_err++;
        return 1u;
    }

    return 0u;
}

uint8_t i2c_bus_read(uint8_t addr, uint8_t *buf, uint16_t len)
{
    if (HAL_I2C_Master_Receive(&hi2c2, (uint16_t)((uint16_t)addr << 1),
                               buf, (uint16_t)len, I2C_BUS_TIMEOUT) != HAL_OK)
    {
        s_err++;
        return 1u;
    }

    return 0u;
}

uint8_t i2c_bus_mem_write(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    if (HAL_I2C_Mem_Write(&hi2c2, (uint16_t)((uint16_t)addr << 1), (uint16_t)reg,
                          I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, (uint16_t)len,
                          I2C_BUS_TIMEOUT) != HAL_OK)
    {
        s_err++;
        return 1u;
    }

    return 0u;
}

uint8_t i2c_bus_mem_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c2, (uint16_t)((uint16_t)addr << 1), (uint16_t)reg,
                         I2C_MEMADD_SIZE_8BIT, buf, (uint16_t)len,
                         I2C_BUS_TIMEOUT) != HAL_OK)
    {
        s_err++;
        return 1u;
    }

    return 0u;
}
