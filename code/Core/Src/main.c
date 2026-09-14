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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

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
/* FatFs 的对象：FATFS 自带一个 512 字节窗口，FIL 也自带一个，
 * 两个加起来约 1.1 KB。而 main() 的栈只有 1 KB
 * （startup_stm32h750xx.s 里 Stack_Size = 0x400）。
 * 放栈上必定溢出，所以放成文件级静态，落到 .bss。 */
static FATFS s_fs;
static DIR   s_dir;
static FIL   s_fil;

static uint32_t g_cycle = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ------------------------------------------------------------------
 * 极简串口输出助手。刻意不用 printf()：标准库的格式化代码会往
 * 128 KB 的片内 Flash 里塞进好几 KB，不值得。
 * ------------------------------------------------------------------ */
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
  /* USER CODE BEGIN 2 */
    /* ---- M1：上电自检，通过 USART1 打印（板载 CH340C）---- */
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

    /* ---- M3：SD 卡 + FATFS -----------------------------------------
     * 下面这个顺序不是排版问题，是被 CubeMX 的两个行为逼出来的：
     *
     * 1) 一旦在 CubeMX 里使能 FATFS，它会把 HAL_SD_Init() 从
     *    MX_SDMMC1_SD_Init() 里删掉 —— 那个函数只剩填 Init 结构体。
     *    卡的识别改由 FATFS 驱动里的 BSP_SD_Init() 负责，
     *    只在第一次 f_mount() 时执行。所以在挂载之前读卡信息，
     *    拿到的全是 0（句柄在 .bss 里，State 是 RESET）。
     *    必须「先挂载，再打印卡信息」。
     *
     * 2) HAL_SD_GetCardInfo() 只拷贝 CardType / CardVersion / Class /
     *    RelCardAdd / BlockNbr / BlockSize / LogBlockNbr / LogBlockSize
     *    这 8 个字段，它不填 CardSpeed。打印 CardSpeed 读到的是
     *    未初始化的栈内存（实测打出一个像地址的天文数字），
     *    所以这里不打印它。 */
    {
        HAL_SD_CardInfoTypeDef ci;
        FRESULT fr;
        DWORD   free_clusters = 0;
        FATFS  *fsp = &s_fs;

        /* ---- 第 1 步：挂载。卡就是在这一步被识别出来的 ---- */
        fr = f_mount(&s_fs, "0:", 1);
        uart_puts("FATFS mount: ");
        if (fr != FR_OK)
        {
            uart_puts("FAILED  fr=");
            uart_put_u32((uint32_t)fr);
            uart_puts("   (13=FR_NO_FILESYSTEM, 3=FR_NOT_READY)\r\n");

            /* HAL 层可能还留着真正的原因 */
            uart_puts("raw HAL    : state=");
            uart_put_u32((uint32_t)hsd1.State);
            uart_puts("  err=0x");
            uart_put_hex32(hsd1.ErrorCode);
            uart_puts("\r\n");
        }
        else
        {
            uart_puts("OK\r\n");

            /* ---- 第 2 步：卡信息 —— 只有第 1 步成功之后才有意义 ---- */
            if (HAL_SD_GetCardInfo(&hsd1, &ci) != HAL_OK)
            {
                uart_puts("SD card    : GetCardInfo FAILED");
                uart_puts("\r\n");
            }
            else
            {
                /* BlockNbr 乘 BlockSize 在 16GB 卡上会超过 32 位，
                 * 所以这个乘法必须用 64 位做。 */
                uint32_t cap_mb = (uint32_t)(((uint64_t)ci.BlockNbr *
                                              (uint64_t)ci.BlockSize)
                                             / (1024ULL * 1024ULL));
                uart_puts("SD card    : OK\r\n");
                uart_puts("SD type    : ");
                uart_put_u32(ci.CardType);    uart_puts("\r\n");
                uart_puts("SD version : ");
                uart_put_u32(ci.CardVersion); uart_puts("\r\n");
                /* 这里打出来的 Class 其实是 CSD 寄存器的 CCC 字段
                 * （Card Command Classes，卡支持的命令类位图）：
                 * HAL 取的是 RESP2 >> 20，也就是 CSD[95:84]。
                 * 0x5B5 = 支持命令类 0/2/4/5/7/8/10，SD 存储卡的标准值。
                 * 注意它跟"速度等级"不是一回事，别混。 */
                uart_puts("SD class   : ");
                uart_put_u32(ci.Class);       uart_puts("\r\n");
                uart_puts("SD blockNbr: ");
                uart_put_u32(ci.BlockNbr);    uart_puts("\r\n");
                uart_puts("SD blockSz : ");
                uart_put_u32(ci.BlockSize);   uart_puts("\r\n");
                uart_puts("SD capacity: ");
                uart_put_u32(cap_mb);         uart_puts(" MB"); uart_puts("\r\n");
            }

            /* ---- 第 3 步：剩余空间 —— 顺带证明 f_getfree 能用 ---- */
            fr = f_getfree("0:", &free_clusters, &fsp);
            if (fr == FR_OK)
            {
                /* free_clusters 的单位是簇；一簇等于 s_fs.csize 个扇区，
                 * 每个扇区 512 字节。 */
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

            /* ---- 第 4 步：列出根目录 ---- */
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

            /* ---- 第 5 步：写一个文件，读回来逐字节比对 ---------------
             * 不需要读卡器：固件自己验证自己的写入 —— 写完再读回来
             * 和原文比对。 */
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
                    if (fr == FR_OK) fr = f_sync(&s_fil);   /* 立刻落盘，不等到 close */
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
        }
    }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	/* ---- M2：RGB 三色轮闪（低电平点亮：写 RESET 就是亮）---- */
	HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);  /* 红 亮 */
	HAL_Delay(300);
	HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);    /* 红 灭 */

	HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);  /* 绿 亮 */
	HAL_Delay(300);
	HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);    /* 绿 灭 */

	HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);  /* 蓝 亮 */
	HAL_Delay(300);
	HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);    /* 蓝 灭 */

	/* ---- 心跳：每完成一整轮 RGB 打印一行 ---- */
	g_cycle++;
	uart_puts("cycle ");
	uart_put_u32((uint32_t)g_cycle);
	uart_puts("\r\n");

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
  /* 死之前先把失败原因打出来。静默死等是最贵的一种现象：
   * 串口没有输出、灯也不闪，看着像板子坏了或者固件根本没烧进去。
   * 所以这里改成先打印，再 __disable_irq()。 */
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
