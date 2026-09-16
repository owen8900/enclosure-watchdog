/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "sdmmc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "ili9341.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* FatFs 对象约 1.1KB，而 main() 的栈只有 1KB（Stack_Size = 0x400），
 * 放栈上会溢出，所以放文件级静态。 */
static FATFS s_fs;
static DIR   s_dir;
static FIL   s_fil;
/* 调试用文件对象（dump / 取大小）。FIL 有 560 字节，必须静态，不能上栈。 */
static FIL   s_dbg;

static uint32_t g_cycle = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 极简串口输出：不用 printf()，省下标准库格式化代码的几 KB Flash。 */
static void uart_puts(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 1000);
}

static void uart_put_u32(uint32_t v)
{
    char buf[12];
    char tmp[10];
    int  i = 0, j;

    if (v == 0u)
    {
        buf[0] = '0';
        buf[1] = '\0';
    }
    else
    {
        while (v > 0u && i < 10)
        {
            tmp[i++] = (char)('0' + (int)(v % 10u));
            v /= 10u;
        }
        for (j = 0; j < i; j++)
        {
            buf[j] = tmp[i - 1 - j];
        }
        buf[i] = '\0';
    }
    uart_puts(buf);
}

static void uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[9];
    int  i;

    for (i = 7; i >= 0; i--)
    {
        buf[i] = hex[v & 0xFu];
        v >>= 4;
    }
    buf[8] = '\0';
    uart_puts(buf);
}

/* uint32 -> 十进制 ASCII，写进 out[]，返回字节数（不写结尾 '\0'）。
 * uint32 十进制最多 10 位（4294967295），tmp 只做倒序暂存。 */
static int u32_to_str(uint32_t v, char *out)
{
    char tmp[10];
    int  i = 0;
    int  j;

    if (v == 0u)
    {
        out[0] = '0';
        return 1;
    }

    while (v > 0u)
    {
        tmp[i] = (char)('0' + (int)(v % 10u));
        i++;
        v /= 10u;
    }

    for (j = 0; j < i; j++)
    {
        out[j] = tmp[i - 1 - j];
    }

    return i;
}

/* 拼一行心跳： "cycle <n>\r\n"，返回字节数（不含结尾 '\0'，buf 至少 20 字节）。 */
static int build_cycle_line(uint32_t n, char *buf)
{
    int len;

    memcpy(buf, "cycle ", 6);
    len = 6;

    len += u32_to_str(n, buf + len);

    buf[len++] = '\r';
    buf[len++] = '\n';
    buf[len]   = '\0';

    return len;
}



/* 取文件字节数（只读打开→读大小→关闭）。打不开时返回 0xFFFFFFFF。 */
static uint32_t file_size_of(const char *path)
{
    FRESULT  fr;
    uint32_t sz;

    fr = f_open(&s_dbg, path, FA_READ);
    if (fr != FR_OK)
    {
        return 0xFFFFFFFFu;
    }
    sz = (uint32_t)f_size(&s_dbg);
    f_close(&s_dbg);

    return sz;
}

/* 把文件开头 max 字节从串口 dump 出来 —— 手边没读卡器时的替代验证手段。
 * 用 HAL_UART_Transmit 定长发，不走 uart_puts：读回来的数据没有结尾 '\0'。 */
static void file_dump_head(const char *path, uint32_t max)
{
    FRESULT  fr;
    UINT     br;
    uint32_t total = 0;
    uint32_t left;
    UINT     want;
    char     tmp[64];

    fr = f_open(&s_dbg, path, FA_READ);
    if (fr != FR_OK)
    {
        uart_puts("--- "); uart_puts(path);
        uart_puts(" : open FAILED fr="); uart_put_u32((uint32_t)fr); uart_puts("\r\n");
        return;
    }

    uart_puts("--- "); uart_puts(path); uart_puts(" size=");
    uart_put_u32((uint32_t)f_size(&s_dbg));
    uart_puts(" B, head ---\r\n");

    while (total < max)
    {
        left = max - total;
        want = (left < (uint32_t)sizeof(tmp)) ? (UINT)left : (UINT)sizeof(tmp);

        br = 0;
        fr = f_read(&s_dbg, tmp, want, &br);
        if (fr != FR_OK || br == 0)
        {
            break;
        }
        HAL_UART_Transmit(&huart1, (uint8_t *)tmp, (uint16_t)br, 1000);
        total += br;
    }

    uart_puts("\r\n--- dump end, "); uart_put_u32(total); uart_puts(" B shown ---\r\n");

    f_close(&s_dbg);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
    /* M1：上电自检（USART1，板载 CH340C） */
    uart_puts("\r\n\r\n");
    uart_puts("========================================\r\n");
    uart_puts("  Cabinet Guardian - Cabinet Health Monitor\r\n");
    uart_puts("  Power-On Self Test (POST)\r\n");
    uart_puts("========================================\r\n");
    uart_puts("MCU        : STM32H750VBTx\r\n");
    uart_puts("SYSCLK     : "); uart_put_u32(HAL_RCC_GetSysClockFreq()); uart_puts(" Hz\r\n");
    uart_puts("HCLK (AXI) : "); uart_put_u32(HAL_RCC_GetHCLKFreq());    uart_puts(" Hz\r\n");
    uart_puts("PCLK1(APB1): "); uart_put_u32(HAL_RCC_GetPCLK1Freq());   uart_puts(" Hz\r\n");
    uart_puts("PCLK2(APB2): "); uart_put_u32(HAL_RCC_GetPCLK2Freq());   uart_puts(" Hz\r\n");
    uart_puts("UART       : 115200 8N1\r\n");
    uart_puts("----------------------------------------\r\n");
    uart_puts("RGB LED    : cycling RED -> GREEN -> BLUE\r\n\r\n");

    /* M4：TFT 屏自检（红 -> 绿 -> 蓝 各 0.4 秒，最后清黑）
     * 串口打印字符串保持 ASCII，避免不同终端乱码。 */
    {
        uint8_t lcd_err;

        uart_puts("TFT LCD    : ");
        lcd_err = ili9341_init();
        if (lcd_err == 0)
        {
            uart_puts("init OK");
        }
        else
        {
            uart_puts("init ERR x");
            uart_put_u32(lcd_err);
        }
        uart_puts("  SPI err total=");
        uart_put_u32(ili9341_error_count());
        uart_puts("\r\n");

        ili9341_fill(ILI9341_RED);
        HAL_Delay(400);
        ili9341_fill(ILI9341_GREEN);
        HAL_Delay(400);
        ili9341_fill(ILI9341_BLUE);
        HAL_Delay(400);
        ili9341_fill(ILI9341_BLACK);

        uart_puts("TFT color  : RED -> GREEN -> BLUE -> BLACK\r\n");
    }

    /* M3：SD 卡 + FATFS
     * 顺序有讲究：CubeMX 使能 FATFS 后，不再由 sdmmc.c 初始化卡，改由 FATFS 的
     * BSP_SD_Init() 在首次 f_mount 时识别，所以卡信息必须挂载之后再读 ——
     * 读早了 hsd1 还是全 0。
     * 另：HAL_SD_GetCardInfo() 不填 CardSpeed，打印它会读到未初始化的栈内存。 */
    {
        HAL_SD_CardInfoTypeDef ci;
        FRESULT fr;
        DWORD   free_clusters = 0;
        FATFS  *fsp = &s_fs;

        /* 1. 挂载 —— 卡在这一步被识别 */
        fr = f_mount(&s_fs, "0:", 1);
        uart_puts("FATFS mount: ");
        if (fr != FR_OK)
        {
            uart_puts("FAILED  fr=");
            uart_put_u32((uint32_t)fr);
            uart_puts("   (13=FR_NO_FILESYSTEM, 3=FR_NOT_READY)\r\n");

            /* HAL 层可能还留着原因 */
            uart_puts("raw HAL    : state=");
            uart_put_u32((uint32_t)hsd1.State);
            uart_puts("  err=0x");
            uart_put_hex32(hsd1.ErrorCode);
            uart_puts("\r\n");
        }
        else
        {
            uart_puts("OK\r\n");

            /* 2. 卡信息（挂载之后才有效） */
            if (HAL_SD_GetCardInfo(&hsd1, &ci) != HAL_OK)
            {
                uart_puts("SD card    : GetCardInfo FAILED");
                uart_puts("\r\n");
            }
            else
            {
                /* 16GB 卡上 BlockNbr 乘 BlockSize 超过 32 位，必须用 64 位算。 */
                uint32_t cap_mb = (uint32_t)(((uint64_t)ci.BlockNbr *
                                              (uint64_t)ci.BlockSize)
                                             / (1024ULL * 1024ULL));
                uart_puts("SD card    : OK\r\n");
                uart_puts("SD type    : ");
                uart_put_u32(ci.CardType);    uart_puts("\r\n");
                uart_puts("SD version : ");
                uart_put_u32(ci.CardVersion); uart_puts("\r\n");
                /* Class 是 CSD 的 CCC 命令类位图（HAL 取 RESP2>>20），不是速度等级。
                 * 0x5B5 = 支持命令类 0/2/4/5/7/8/10。 */
                uart_puts("SD class   : ");
                uart_put_u32(ci.Class);       uart_puts("\r\n");
                uart_puts("SD blockNbr: ");
                uart_put_u32(ci.BlockNbr);    uart_puts("\r\n");
                uart_puts("SD blockSz : ");
                uart_put_u32(ci.BlockSize);   uart_puts("\r\n");
                uart_puts("SD capacity: ");
                uart_put_u32(cap_mb);         uart_puts(" MB"); uart_puts("\r\n");
            }

            /* 3. 剩余空间 */
            fr = f_getfree("0:", &free_clusters, &fsp);
            if (fr == FR_OK)
            {
                /* free_clusters 单位是簇；一簇 = s_fs.csize 个 512 字节扇区。 */
                uint32_t free_mb = (uint32_t)(((uint64_t)free_clusters *
                                              (uint64_t)s_fs.csize * 512ULL)
                                             / (1024ULL * 1024ULL));
                uart_puts("FATFS free : ");
                uart_put_u32(free_mb);
                uart_puts(" MB   (cluster = ");
                uart_put_u32((uint32_t)s_fs.csize);
                uart_puts(" sectors)\r\n");
            }
            else
            {
                uart_puts("FATFS free : FAILED  fr=");
                uart_put_u32((uint32_t)fr);
                uart_puts("\r\n");
            }

            /* 4. 列根目录 */
            {
                FILINFO  fno;
                uint32_t n = 0;

                fr = f_opendir(&s_dir, "0:/");
                if (fr != FR_OK)
                {
                    uart_puts("FATFS dir  : FAILED  fr=");
                    uart_put_u32((uint32_t)fr);
                    uart_puts("\r\n");
                }
                else
                {
                    uart_puts("FATFS dir  :\r\n");
                    for (;;)
                    {
                        fr = f_readdir(&s_dir, &fno);
                        if (fr != FR_OK || fno.fname[0] == 0) break;
                        uart_puts("  ");
                        if (fno.fattrib & AM_DIR) uart_puts("[DIR ] ");
                        else                      uart_puts("[FILE] ");
                        uart_puts(fno.fname);
                        uart_puts("\r\n");
                        n++;
                    }
                    f_closedir(&s_dir);
                    uart_puts("FATFS dir  : ");
                    uart_put_u32(n);
                    uart_puts(" entries\r\n");
                }
            }

            /* 5. 写文件再读回来逐字节比对 —— 固件自己验证自己的写入，
             * 不需要读卡器。 */
            {
                UINT bw = 0, br = 0;
                static const char LINE[] = "Cabinet Guardian FATFS self test\r\n";
                char rd[sizeof(LINE)];

                fr = f_open(&s_fil, "0:/CG_TEST.TXT", FA_CREATE_ALWAYS | FA_WRITE);
                if (fr != FR_OK)
                {
                    uart_puts("FATFS write: FAILED (open) fr=");
                    uart_put_u32((uint32_t)fr);
                    uart_puts("\r\n");
                }
                else
                {
                    fr = f_write(&s_fil, LINE, sizeof(LINE) - 1, &bw);
                    if (fr == FR_OK) fr = f_sync(&s_fil);   /* 立刻落盘 */
                    f_close(&s_fil);

                    if (fr != FR_OK || bw != sizeof(LINE) - 1)
                    {
                        uart_puts("FATFS write: FAILED  fr=");
                        uart_put_u32((uint32_t)fr);
                        uart_puts("  wrote=");
                        uart_put_u32((uint32_t)bw);
                        uart_puts("\r\n");
                    }
                    else
                    {
                        uart_puts("FATFS write: ");
                        uart_put_u32((uint32_t)bw);
                        uart_puts(" bytes -> CG_TEST.TXT\r\n");

                        fr = f_open(&s_fil, "0:/CG_TEST.TXT", FA_READ);
                        if (fr == FR_OK)
                        {
                            fr = f_read(&s_fil, rd, sizeof(LINE) - 1, &br);
                            f_close(&s_fil);
                            rd[sizeof(LINE) - 1] = 0;
                            if (fr == FR_OK && br == sizeof(LINE) - 1 &&
                                memcmp(rd, LINE, sizeof(LINE) - 1) == 0)
                            {
                                uart_puts("FATFS read : OK, content matches\r\n");
                            }
                            else
                            {
                                uart_puts("FATFS read : MISMATCH\r\n");
                            }
                        }
                    }
                }
            }

            /* 6. 日志文件开头 dump 一次：验证「上一轮跑完，卡里到底存了什么」。
             * 没读卡器时就看这里。 */
            file_dump_head("0:/LOG.TXT", 200);
        }
    }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	/* M2：RGB 轮闪（低电平点亮） */
	HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);  /* 红亮 */
	HAL_Delay(300);
	HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);    /* 红灭 */

	HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);  /* 绿亮 */
	HAL_Delay(300);
	HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);    /* 绿灭 */

	HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);  /* 蓝亮 */
	HAL_Delay(300);
	HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);    /* 蓝灭 */

	/* 心跳：每轮 RGB 打一行；同一行内容后面要追加进 SD 卡日志 */
	g_cycle++;
	{
		char line[20];                              /* 6 + 10 + 2 + 1 = 19 */
		int  n = build_cycle_line(g_cycle, line);

		uart_puts(line);                            /* 串口输出与改前逐字节相同 */

	
		{
			FRESULT fr;
			UINT    bw = 0;

			fr = f_open(&s_fil, "0:/LOG.TXT", FA_WRITE | FA_OPEN_APPEND);   
			if (fr != FR_OK)
			{


                    uart_puts("FATFS write: FAILED (open) fr=");
                    uart_put_u32((uint32_t)fr);
                    uart_puts("\r\n ");
			}
			else
			{
				fr = f_write(&s_fil, line, n, &bw);      
				if (fr != FR_OK || bw != n)             
				{
		
                    uart_puts("FATFS write: FAILED  fr=");
                    uart_put_u32((uint32_t)fr);
                    uart_puts("  wrote=");
                    uart_put_u32((uint32_t)bw);
                    uart_puts("\r\n ");
				}
				f_close(&s_fil);                            
			}
		}

		/* 调试：日志文件当前大小。追加 → 每轮变大；覆盖 → 一直不变。（验完可删） */
		uart_puts("LOG size = ");
		uart_put_u32(file_size_of("0:/LOG.TXT"));
		uart_puts(" B\r\n");
	}

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 120;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 死之前先打印原因：静默死等会让人误判成板子坏了或固件没烧进去。 */
  uart_puts("\r\n*** FATAL: Error_Handler() ***\r\n");
  uart_puts("SD  err=0x"); uart_put_hex32(hsd1.ErrorCode);
  uart_puts("  state=");   uart_put_u32((uint32_t)hsd1.State);
  uart_puts("\r\n");

  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
