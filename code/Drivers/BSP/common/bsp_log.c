/**
  ******************************************************************************
  * 文件    : bsp_log.c
  * 功能    : BSP 层共用的串口日志出口实现
  * 说明    : 只有一处回调，避免每个模块各存一份函数指针
  ******************************************************************************
  */

#include "bsp_log.h"

static void (*s_log)(const char *) = 0;

void bsp_log_set(void (*log)(const char *s))
{
    s_log = log;
}

void bsp_log(const char *s)
{
    if (s_log != 0)
    {
        s_log(s);
    }
}

static void log_char(char c)
{
    char b[2];

    b[0] = c;
    b[1] = 0;
    bsp_log(b);
}

void bsp_log_u32(uint32_t v)
{
    char t[10];
    int  i = 0;

    if (v == 0u)
    {
        log_char('0');
        return;
    }
    while (v > 0u && i < 10)
    {
        t[i++] = (char)('0' + (char)(v % 10u));
        v /= 10u;
    }
    while (i > 0)
    {
        log_char(t[--i]);
    }
}

void bsp_log_hex2(uint8_t v)
{
    static const char hx[] = "0123456789ABCDEF";

    log_char(hx[(v >> 4) & 0x0Fu]);
    log_char(hx[v & 0x0Fu]);
}

void bsp_log_milli(int32_t milli)
{
    uint32_t v;
    uint32_t frac;

    if (milli < 0)
    {
        bsp_log("-");
        v = (uint32_t)(-milli);
    }
    else
    {
        v = (uint32_t)milli;
    }

    bsp_log_u32(v / 1000u);
    bsp_log(".");
    frac = v % 1000u;
    bsp_log_u32(frac / 100u);
    bsp_log_u32((frac / 10u) % 10u);
    bsp_log_u32(frac % 10u);
}

void bsp_log_ascii(const uint8_t *buf, uint16_t n)
{
    uint16_t i;

    for (i = 0u; i < n; i++)
    {
        uint8_t b = buf[i];

        if ((b == (uint8_t)'\r') || (b == (uint8_t)'\n'))
        {
            continue;
        }
        log_char(((b >= 0x20u) && (b < 0x7Fu)) ? (char)b : '.');
    }
}
