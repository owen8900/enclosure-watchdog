/**
  ******************************************************************************
  * 文件    : sht30.c
  * 功能    : SHT30 温湿度读取（单次测量 + CRC8 校验）
  * 说明    : 命令字、时延、换算公式全部照 Sensirion SHT3x-DIS 数据手册 v7：
  *           0x2400 = 高重复度、不开时钟拉伸，测量最长 15.5ms，这里等 20ms；
  *           读完用独立的 START 再发读头，回 6 字节 T_MSB T_LSB T_CRC RH_MSB
  *           RH_LSB RH_CRC；CRC8 多项式 0x31、初值 0xFF、不反射、末异或 0，
  *           手册给的测例是 CRC(0xBEEF) = 0x92
  * 约定    : 每次收发都带超时，失败只返回错误码并计数，不阻塞
  ******************************************************************************
  */

#include "sht30.h"
#include "bsp_log.h"
#include "i2c_bus.h"
#include "i2c.h"

#define SHT30_MEAS_MS       20u         /* 手册 12.5ms 典型 / 15.5ms 最大 */

/* 返回码：1 命令没被应答 2 读数据失败 3 温度 CRC 错 4 湿度 CRC 错 */
#define SHT30_OK            0u
#define SHT30_ERR_CMD       1u
#define SHT30_ERR_READ      2u
#define SHT30_ERR_CRCT      3u
#define SHT30_ERR_CRCR      4u

static uint8_t crc8(const uint8_t *p, uint8_t n)
{
    uint8_t crc = 0xFFu;
    uint8_t i;
    uint8_t b;

    for (i = 0u; i < n; i++)
    {
        crc ^= p[i];
        for (b = 0u; b < 8u; b++)
        {
            if ((crc & 0x80u) != 0u)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31u);
            }
            else
            {
                crc = (uint8_t)(crc << 1);
            }
        }
    }

    return crc;
}

uint8_t sht30_probe(uint8_t addr)
{
    if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)((uint16_t)addr << 1), 1u, 5u) == HAL_OK)
    {
        return 0u;
    }

    return 1u;
}

uint8_t sht30_read(uint8_t addr, sht30_data_t *d)
{
    uint8_t  cmd[2];
    uint8_t  rx[6];
    uint16_t s_t;
    uint16_t s_rh;

    if (d == 0)
    {
        return SHT30_ERR_READ;
    }

    cmd[0] = 0x24u;
    cmd[1] = 0x00u;

    if (i2c_bus_write(addr, cmd, 2u) != 0u)
    {
        return SHT30_ERR_CMD;
    }

    HAL_Delay(SHT30_MEAS_MS);

    if (i2c_bus_read(addr, rx, 6u) != 0u)
    {
        return SHT30_ERR_READ;
    }

    if (crc8(&rx[0], 2u) != rx[2])
    {
        return SHT30_ERR_CRCT;
    }
    if (crc8(&rx[3], 2u) != rx[5])
    {
        return SHT30_ERR_CRCR;
    }

    s_t  = (uint16_t)(((uint16_t)rx[0] << 8) | (uint16_t)rx[1]);
    s_rh = (uint16_t)(((uint16_t)rx[3] << 8) | (uint16_t)rx[4]);

    d->temp_milli = (int32_t)(((int64_t)175000 * (int64_t)s_t + 32767) / 65535) - 45000;
    d->rh_milli   = (int32_t)(((int64_t)100000 * (int64_t)s_rh + 32767) / 65535);

    return SHT30_OK;
}

static void report(uint8_t addr)
{
    sht30_data_t d;
    uint8_t rc;

    bsp_log("SHT30 @0x");
    bsp_log_hex2(addr);
    bsp_log(": ");

    rc = sht30_read(addr, &d);
    if (rc != SHT30_OK)
    {
        bsp_log("read ERR rc=");
        bsp_log_u32((uint32_t)rc);
        bsp_log("  (1=ack 2=read 3=T-crc 4=RH-crc)\r\n");
        return;
    }

    bsp_log("T=");
    bsp_log_milli(d.temp_milli);
    bsp_log(" C  RH=");
    bsp_log_milli(d.rh_milli);
    bsp_log(" %\r\n");
}

void sht30_selftest(void)
{
    report(SHT30_ADDR_1);
    report(SHT30_ADDR_2);
}

void sht30_poll(void)
{
    report(SHT30_ADDR_1);
    report(SHT30_ADDR_2);
}
