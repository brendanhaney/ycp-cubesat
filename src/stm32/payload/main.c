/**
******************************************************************************
* @file    main.c
* @author  GPM Application Team
*
******************************************************************************
* @attention
*
* Copyright (c) 2023 STMicroelectronics.
* All rights reserved.
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
*
******************************************************************************
*/

#include <assert.h>

#include "app.h"
#include "app_fuseprogramming.h"
#include "main.h"
#include "stm32n6xx_nucleo.h"
#include <stdio.h>
#include <string.h>
#include "stm32n6xx_hal_rif.h"
#include "app_config.h"

#include "storage.h"
#include "payload_spi.h"
#include "image_server.h"

extern uint8_t _sdata;
extern uint8_t __uncached_bss_start__;
extern uint8_t __uncached_bss_end__;

UART_HandleTypeDef huart1;
volatile uint32_t debug_spi_requests = 0;
volatile uint32_t debug_spi_errors = 0;
static int spi_initialized;
static uint8_t spi_request[IMAGE_SPI_FRAME_SIZE];
static uint8_t spi_response[IMAGE_SPI_FRAME_SIZE];
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

static void SystemClock_Config(void);
static void Security_Config();
static void IAC_Config();
static void CONSOLE_Config(void);
static void Setup_Mpu(void);
static void board_init(void);
static void service_payload_spi(void);

/**
 * @brief  Main program
 * @param  None
 * @retval None
 */
int main(void) {
    /* The FSBL SystemInit gates SYSCFG off. ST-LINK needs this clock left
     * enabled to reconnect over AP1 after execution has left the boot ROM. */
    __HAL_RCC_SYSCFG_CLK_ENABLE();

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

    __enable_irq();

    board_init();

    spi_initialized = payload_spi_init() == 0;

    if (!spi_initialized) {

        debug_spi_errors++;

        printf("Payload SPI init failed\n");

    }

    // Send image to comms over SPI
    image_server_init(spi_response);

    payload_spi_set_response(spi_response);

    debug_capture_status = payload_camera_init();

    if (debug_capture_status != PAYLOAD_OK) {

        BSP_LED_On(LED_RED);

        while (1) {
            service_payload_spi();
            image_server_transfer_active();
            __WFI();
        }

    }

    uint32_t last_capture = HAL_GetTick();

    while (1) {

        /* Process INFO/READ before deciding whether the next capture can run. */
        service_payload_spi();

        if (image_server_transfer_active()) {
            /* Leave debugger capture requests pending too: app_capture_step
             * would consume them even if automatic_capture were false. */
            payload_camera_poll();
        } else {
            const uint32_t now = HAL_GetTick();
            const int automatic_capture = !debug_capture_request && !debug_capture_ready &&
                (uint32_t)(now - last_capture) >= CAPTURE_INTERVAL_MS;
            const int capture_requested = automatic_capture || debug_capture_request;

            if (capture_requested)
                payload_spi_pause();

            app_capture_step(automatic_capture);

            /* Leave a service window even when a save exceeds the interval. */
            if (capture_requested)
                last_capture = HAL_GetTick();
        }

        // SPI requests wait until the complete save finishes.
        service_payload_spi();

        __WFI();

    }
}

static void service_payload_spi(void) {

    if (!spi_initialized)
        return;

    const int result = payload_spi_service(spi_request);

    if (result < 0) {

        debug_spi_errors++;

        payload_spi_service(spi_request);

        return;
    }

    if (result > 0) {

        debug_spi_requests++;

        if (image_server_handle(spi_request, spi_response))
            payload_spi_set_response(spi_response);

        /* Preload the next reply before sleeping or starting another capture. */
        payload_spi_service(spi_request);
    }
}

static void Setup_MpuRegion(uint8_t number, uint32_t start, uint32_t end) {
    MPU_Region_InitTypeDef region = {0};

    region.Enable = MPU_REGION_ENABLE;
    region.Number = number;
    region.BaseAddress = start & ~31U;
    region.LimitAddress = (end - 1U) | 31U;
    region.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
    region.AccessPermission = MPU_REGION_ALL_RW;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    region.DisablePrivExec = MPU_PRIV_INSTRUCTION_ACCESS_ENABLE;
    region.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    HAL_MPU_ConfigRegion(&region);
}

static void Setup_Mpu(void) {
    MPU_Attributes_InitTypeDef attr = {0};

    attr.Number = MPU_ATTRIBUTES_NUMBER0;
    attr.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
    HAL_MPU_ConfigMemoryAttributes(&attr);

    /* Keep executable code cached. Shared data is noncacheable so ST-LINK
     * requests/status and DCMIPP frame writes remain visible to the CPU. */
    Setup_MpuRegion(MPU_REGION_NUMBER0, (uint32_t)&_sdata, 0x34200000UL);
    Setup_MpuRegion(MPU_REGION_NUMBER1, 0x34000000UL, 0x34180000UL);
    Setup_MpuRegion(MPU_REGION_NUMBER2, 0x34200000UL, 0x343C0000UL);
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
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_SDMMC2, &RIMC_master);

    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP,
                                          RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_JPEG,
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

    /* System clock (sysc_ck) = ic6_ck = PLL2 output/ic6_divider = 1000 MHz */
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

static void board_init(void) {
    int ret;

    for (IRQn_Type i = PVD_PVM_IRQn; i <= LTDC_UP_ERR_IRQn; i++)
        HAL_NVIC_SetPriority(i, 6, 0);
    SystemClock_Config();

    CONSOLE_Config();

    Fuse_Programming();

    ret = BSP_LED_Init(LED_GREEN);
    assert(ret == BSP_ERROR_NONE);

    ret = BSP_LED_Init(LED_RED);
    assert(ret == BSP_ERROR_NONE);

    /* Set all required IPs as secure privileged */
    Security_Config();
    payload_storage_init();

    /* Experimental AXISRAM3 initialization and CPU access test */
    __HAL_RCC_RAMCFG_CLK_ENABLE();

    /* Power up SRAM3 using its secure register alias */
    CLEAR_BIT(RAMCFG_SRAM3_AXI_S->CR, RAMCFG_CR_SRAMSD);
    HAL_Delay(1);

    /* Enable the SRAM3 memory clock */
    __HAL_RCC_AXISRAM3_MEM_CLK_ENABLE();

    /* Write and read a test pattern */
    volatile uint32_t *sram3_test = (volatile uint32_t *)0x34200000UL;

    *sram3_test = 0xA5C35A3CU;
    __DSB();

    debug_sram3_readback = *sram3_test;
    debug_sram3_pass = (debug_sram3_readback == 0xA5C35A3CU);

    /* Enable and test AXISRAM4 */
    CLEAR_BIT(RAMCFG_SRAM4_AXI_S->CR, RAMCFG_CR_SRAMSD);
    HAL_Delay(1);

    __HAL_RCC_AXISRAM4_MEM_CLK_ENABLE();

    /* Test SRAM4 access */
    volatile uint32_t *sram4_test = (volatile uint32_t *)0x34270000UL;

    *sram4_test = 0x1234ABCDU;
    __DSB();

    debug_sram4_readback = *sram4_test;
    debug_sram4_pass = (debug_sram4_readback == 0x1234ABCDU);

    /* Enable AXISRAM5 */
    CLEAR_BIT(RAMCFG_SRAM5_AXI_S->CR, RAMCFG_CR_SRAMSD);
    HAL_Delay(1);
    __HAL_RCC_AXISRAM5_MEM_CLK_ENABLE();

    volatile uint32_t *sram5_test = (volatile uint32_t *)0x342E0000UL;

    *sram5_test = 0x55AA1234U;
    __DSB();

    debug_sram5_readback = *sram5_test;
    debug_sram5_pass = (debug_sram5_readback == 0x55AA1234U);

    /* Enable and test AXISRAM6 */
    CLEAR_BIT(RAMCFG_SRAM6_AXI_S->CR, RAMCFG_CR_SRAMSD);
    HAL_Delay(1);
    __HAL_RCC_AXISRAM6_MEM_CLK_ENABLE();

    volatile uint32_t *sram6_test = (volatile uint32_t *)0x34350000UL;

    *sram6_test = 0xAA556789U;
    __DSB();

    debug_sram6_readback = *sram6_test;
    debug_sram6_pass = (debug_sram6_readback == 0xAA556789U);

    /* Test a full 800x480 YUV422 buffer across SRAM3 and SRAM4 */
    {
        volatile uint32_t *buffer = (volatile uint32_t *)0x34200000UL;
        const uint32_t words = (800U * 480U * 2U) / 4U;
        uint32_t i;

        /* Write test pattern */
        for (i = 0; i < words; i++) {
            buffer[i] = 0x5A000000U ^ i;
        }

        __DSB();

        /* Verify every word */
        debug_span34_pass = 1;

        for (i = 0; i < words; i++) {
            if (buffer[i] != (0x5A000000U ^ i)) {
                debug_span34_pass = 0;
                debug_span34_first_bad = i;
                break;
            }
        }

        debug_span34_done = 1;
    }

    /* Test full 800x480 buffer across SRAM5 and SRAM6 */
    {
        volatile uint32_t *buffer = (volatile uint32_t *)0x342E0000UL;
        const uint32_t words = (800U * 480U * 2U) / 4U;
        uint32_t i;

        /* Write test pattern */
        for (i = 0; i < words; i++) {
            buffer[i] = 0xA5000000U ^ i;
        }

        __DSB();

        /* Verify every word */
        debug_span56_pass = 1;

        for (i = 0; i < words; i++) {
            if (buffer[i] != (0xA5000000U ^ i)) {
                debug_span56_pass = 0;
                debug_span56_first_bad = i;
                break;
            }
        }

        debug_span56_done = 1;
    }

    IAC_Config();

    /* Keep all IP's enabled during WFE so they can wake up CPU. Fine tune
     * this if you want to save maximum power
     */
    LL_BUS_EnableClockLowPower(~0);
    LL_MEM_EnableClockLowPower(~0);
    LL_AHB1_GRP1_EnableClockLowPower(~0);
    LL_AHB2_GRP1_EnableClockLowPower(~0);
    LL_AHB3_GRP1_EnableClockLowPower(~0);
    LL_AHB4_GRP1_EnableClockLowPower(~0);
    LL_AHB5_GRP1_EnableClockLowPower(~0);
    LL_APB1_GRP1_EnableClockLowPower(~0);
    LL_APB1_GRP2_EnableClockLowPower(~0);
    LL_APB2_GRP1_EnableClockLowPower(~0);
    LL_APB4_GRP1_EnableClockLowPower(~0);
    LL_APB4_GRP2_EnableClockLowPower(~0);
    LL_APB5_GRP1_EnableClockLowPower(~0);
    LL_MISC_EnableClockLowPower(~0);

}

HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp) {
    RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};
    HAL_StatusTypeDef ret;

    RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
    RCC_PeriphCLKInitStruct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC17].ClockDivider = 3;
    ret = HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);
    if (ret)
        return ret;

    RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CSI;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC18].ClockDivider = 40;
    ret = HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);
    if (ret)
        return ret;

    return HAL_OK;
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
    UNUSED(file);
    UNUSED(line);
    __BKPT(0);
    while (1) {
    }
}
#endif
