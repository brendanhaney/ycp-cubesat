#include <drivers.h>
#include <utils.h>
#include "utils/spi_demo.h"

#include <assert.h>

#include "app.h"
#include "app_config.h"
#include "app_fuseprogramming.h"
#include "main.h"
#include "npu_cache.h"
#ifdef STM32N6570_DK_REV
#include "stm32n6570_discovery.h"
#include "stm32n6570_discovery_bus.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6570_discovery_xspi.h"
#else
#include "stm32n6xx_nucleo.h"
#include "stm32n6xx_nucleo_bus.h"
#include "stm32n6xx_nucleo_xspi.h"
#endif
#include <stdio.h>
#include <string.h>
#include "stm32n6xx_hal_rif.h"
#include "FreeRTOS.h"
#include "task.h"

#include "storage.h"

extern uint8_t __uncached_bss_start__;
extern uint8_t __uncached_bss_end__;

UART_HandleTypeDef huart1;
static volatile uint32_t debug_sram3_readback = 0;
static volatile uint32_t debug_sram3_pass = 0;

static volatile uint32_t debug_sram4_readback = 0;
static volatile uint32_t debug_sram4_pass = 0;

static volatile uint32_t debug_sram5_readback = 0;
static volatile uint32_t debug_sram5_pass = 0;

static volatile uint32_t debug_sram6_readback = 0;
static volatile uint32_t debug_sram6_pass = 0;

static volatile uint32_t debug_span34_done = 0;
static volatile uint32_t debug_span34_pass = 0;
static volatile uint32_t debug_span34_first_bad = 0xFFFFFFFFU;

static volatile uint32_t debug_span56_done = 0;
static volatile uint32_t debug_span56_pass = 0;
static volatile uint32_t debug_span56_first_bad = 0xFFFFFFFFU;

static StaticTask_t main_thread;
static StackType_t main_thread_stack[configMINIMAL_STACK_SIZE];

static void SystemClock_Config(void);
static void Security_Config();
static void IAC_Config();
static void CONSOLE_Config(void);
static void Setup_Mpu(void);
static int main_freertos(void);
static void main_thread_fct(void *arg);

/* This is defined in port.c */
void vPortSetupTimerInterrupt(void);

/**
 * @brief  Main program
 * @param  None
 * @retval None
 */
int main(void) {
    /* Power on ICACHE */
    MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_ICACTIVE_Msk;

    /* Set back system and CPU clock source to HSI */
    __HAL_RCC_CPUCLK_CONFIG(RCC_CPUCLKSOURCE_HSI);
    __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);

    HAL_Init();

    Setup_Mpu();

    SCB_EnableICache();

#if defined(USE_DCACHE)
    /* Power on DCACHE */
    MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk;
    SCB_EnableDCache();
#endif

    return main_freertos();
}

static void Setup_Mpu() {
    MPU_Attributes_InitTypeDef attr;
    MPU_Region_InitTypeDef region;

    attr.Number = MPU_ATTRIBUTES_NUMBER0;
    attr.Attributes = MPU_NOT_CACHEABLE;
    HAL_MPU_ConfigMemoryAttributes(&attr);

    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER0;
    region.BaseAddress = (uint32_t)&__uncached_bss_start__;
    region.LimitAddress = (uint32_t)&__uncached_bss_end__ - 1;
    region.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
    region.AccessPermission = MPU_REGION_ALL_RW;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    region.DisablePrivExec = MPU_PRIV_INSTRUCTION_ACCESS_ENABLE;
    region.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    HAL_MPU_ConfigRegion(&region);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

    const size_t uncached_size =
        (uintptr_t)&__uncached_bss_end__ - (uintptr_t)&__uncached_bss_start__;
    memset(&__uncached_bss_start__, 0, uncached_size);
}

static void Security_Config() {
    __HAL_RCC_RIFSC_CLK_ENABLE();
    RIMC_MasterConfig_t RIMC_master = {0};
    RIMC_master.MasterCID = RIF_CID_1;
    RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &RIMC_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D, &RIMC_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &RIMC_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2, &RIMC_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_OTG1, &RIMC_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_SDMMC2, &RIMC_master);

    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_JPEG,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SDMMC2,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
}

static void IAC_Config(void) {
    /* Configure IAC to trap illegal access events */
    __HAL_RCC_IAC_CLK_ENABLE();
    __HAL_RCC_IAC_FORCE_RESET();
    __HAL_RCC_IAC_RELEASE_RESET();
}

void IAC_IRQHandler(void) {
    while (1) {
    }
}

static void SystemClock_Config(void) {
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};

    BSP_SMPS_Init(SMPS_VOLTAGE_OVERDRIVE);
    HAL_Delay(1); /* Assuming Voltage Ramp Speed of 1mV/us --> 100mV increase takes 100us */

    // Oscillator config already done in bootrom
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;

    /* PLL1 = 64 x 25 / 2 = 800MHz */
    RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL1.PLLM = 2;
    RCC_OscInitStruct.PLL1.PLLN = 25;
    RCC_OscInitStruct.PLL1.PLLFractional = 0;
    RCC_OscInitStruct.PLL1.PLLP1 = 1;
    RCC_OscInitStruct.PLL1.PLLP2 = 1;

    /* PLL2 = 64 x 125 / 8 = 1000MHz */
    RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL2.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL2.PLLM = 8;
    RCC_OscInitStruct.PLL2.PLLFractional = 0;
    RCC_OscInitStruct.PLL2.PLLN = 125;
    RCC_OscInitStruct.PLL2.PLLP1 = 1;
    RCC_OscInitStruct.PLL2.PLLP2 = 1;

    /* PLL3 = 64 x 225 / 16 = 900MHz */
    RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL3.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL3.PLLM = 16;
    RCC_OscInitStruct.PLL3.PLLN = 225;
    RCC_OscInitStruct.PLL3.PLLFractional = 0;
    RCC_OscInitStruct.PLL3.PLLP1 = 1;
    RCC_OscInitStruct.PLL3.PLLP2 = 1;

    /* PLL4 = 64 x 20 / 32 = 50MHz */
    RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL4.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL4.PLLM = 32;
    RCC_OscInitStruct.PLL4.PLLFractional = 0;
    RCC_OscInitStruct.PLL4.PLLN = 20;
    RCC_OscInitStruct.PLL4.PLLP1 = 1;
    RCC_OscInitStruct.PLL4.PLLP2 = 1;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        while (1)
            ;
    }

    RCC_ClkInitStruct.ClockType =
        (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 |
         RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK4 | RCC_CLOCKTYPE_PCLK5);

    /* CPU CLock (sysa_ck) = ic1_ck = PLL1 output/ic1_divider = 800 MHz */
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
    RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC1Selection.ClockDivider = 1;

    /* AXI Clock (sysb_ck) = ic2_ck = PLL1 output/ic2_divider = 400 MHz */
    RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC2Selection.ClockDivider = 2;

    /* NPU Clock (sysc_ck) = ic6_ck = PLL2 output/ic6_divider = 1000 MHz */
    RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL2;
    RCC_ClkInitStruct.IC6Selection.ClockDivider = 1;

    /* AXISRAM3/4/5/6 Clock (sysd_ck) = ic11_ck = PLL3 output/ic11_divider = 900 MHz */
    RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL3;
    RCC_ClkInitStruct.IC11Selection.ClockDivider = 1;

    /* HCLK = sysb_ck / HCLK divider = 200 MHz */
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;

    /* PCLKx = HCLK / PCLKx divider = 200 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
    RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK) {
        while (1)
            ;
    }

    RCC_PeriphCLKInitStruct.PeriphClockSelection = 0;

    /* XSPI1 kernel clock (ck_ker_xspi1) = HCLK = 200MHz */
    RCC_PeriphCLKInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_XSPI1;
    RCC_PeriphCLKInitStruct.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_HCLK;

    /* XSPI2 kernel clock (ck_ker_xspi1) = HCLK =  200MHz */
    RCC_PeriphCLKInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_XSPI2;
    RCC_PeriphCLKInitStruct.Xspi2ClockSelection = RCC_XSPI2CLKSOURCE_HCLK;

    if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct) != HAL_OK) {
        while (1)
            ;
    }
}

static void CONSOLE_Config() {
    GPIO_InitTypeDef gpio_init;

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* DISCO & NUCLEO USART1 (PE5/PE6) */
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    gpio_init.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOE, &gpio_init);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_8;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        while (1)
            ;
    }
}

static int main_freertos() {
    TaskHandle_t hdl;

    hdl = xTaskCreateStatic(main_thread_fct, "main", configMINIMAL_STACK_SIZE, NULL,
                            tskIDLE_PRIORITY + 1, main_thread_stack, &main_thread);
    assert(hdl != NULL);

    vTaskStartScheduler();
    assert(0);

    return -1;
}

static void main_thread_fct(void *arg) {
    uint32_t preemptPriority;
    uint32_t subPriority;
    IRQn_Type i;
    int ret;

    if (delay_init(64000000U)) {
        uart_printf("Payload: delay initialization failed\n");
        return 1;
    }

    //spi_demo_slave("Payload", "PAYLD>A", "A>PAYLD");

    while (1) {

        uart_printf("Hello again\n");

        delay_ms(1000U);
    }

/* Allow to debug with cache enable */
__attribute__((section(".keep_me"))) void app_clean_invalidate_dbg() {
    SCB_CleanInvalidateDCache();
}
