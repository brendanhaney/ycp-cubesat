/**
******************************************************************************
* @file    stm32n6xx_hal_conf.h
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32N6xx_HAL_CONF_H
#define STM32N6xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* ########################## Module Selection ############################## */
/**
 * @brief This is the list of modules to be used in the HAL driver
 */
#define HAL_MODULE_ENABLED
#define HAL_BSEC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DCMIPP_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_JPEG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RAMCFG_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RIF_MODULE_ENABLED
#define HAL_SD_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ########################## Oscillator Values adaptation ####################*/
/**
 * @brief Adjust the value of External High Speed oscillator (HSE) used in your application.
 *        This value is used by the RCC HAL module to compute the system frequency
 *        (when HSE is used as system clock source, directly or through the PLL).
 */
#if !defined(HSE_VALUE)
#define HSE_VALUE 48000000UL /*!< Value of the External oscillator in Hz */
#endif                       /* HSE_VALUE */

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100UL /*!< Time out for HSE start up, in ms */
#endif                            /* HSE_STARTUP_TIMEOUT */

/**
 * @brief External Low Speed oscillator (LSE) value.
 *        This value is used by the UART, RTC HAL module to compute the system frequency
 */
#if !defined(LSE_VALUE)
#define LSE_VALUE 32768UL /*!< Value of the External oscillator in Hz */
#endif                    /* LSE_VALUE */

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT 5000UL /*!< Time out for LSE start up, in ms */
#endif                             /* LSE_STARTUP_TIMEOUT */

/**
 * @brief Internal Multiple Speed oscillator (MSI) default value.
 *        This value is the default MSI range value after Reset.
 */
#if !defined(MSI_VALUE)
#define MSI_VALUE 4000000UL /*!< Value of the Internal oscillator in Hz */
#endif                      /* MSI_VALUE */

/**
 * @brief Internal High Speed oscillator (HSI) value.
 *        This value is used by the RCC HAL module to compute the system frequency
 *        (when HSI is used as system clock source, directly or through the PLL).
 */
#if !defined(HSI_VALUE)
#define HSI_VALUE 64000000UL /*!< Value of the Internal oscillator in Hz */
#endif                       /* HSI_VALUE */

/**
 * @brief Internal Low Speed oscillator (LSI) value.
 */
#if !defined(LSI_VALUE)
#define LSI_VALUE 32000UL /*!< LSI Typical Value in Hz*/
#endif /* LSI_VALUE */    /*!< Value of the Internal Low Speed oscillator in Hz                    \
                               The real value may vary depending on the variations in voltage      \
                               and temperature.*/

/* Tip: To avoid modifying this file each time you need to use different HSE,
   ===  you can define the HSE value in your toolchain compiler preprocessor. */

/* ########################### System Configuration ######################### */
/**
 * @brief This is the HAL system configuration section
 */
#define VDD_VALUE 3300UL /*!< Value of VDD in mv */
#define TICK_INT_PRIORITY                                                                          \
    ((1UL << __NVIC_PRIO_BITS) - 1UL) /*!< tick interrupt priority (lowest by default) */
#define USE_RTOS 0U

/* ########################## Assert Selection ############################## */
/**
 * @brief Uncomment the line below to expanse the "assert_param" macro in the
 *        HAL drivers code
 */
/* #define USE_FULL_ASSERT    1U */

/* ################## Register callback feature configuration ############### */
/**
 * @brief Set below the peripheral configuration  to "1U" to add the support
 *        of HAL callback registration/unregistration feature for the HAL
 *        driver(s). This allows user application to provide specific callback
 *        functions thanks to HAL_PPP_RegisterCallback() rather than overwriting
 *        the default weak callback functions (see each stm32n6xx_hal_ppp.h file
 *        for possible callback identifiers defined in HAL_PPP_CallbackIDTypeDef
 *        for each PPP peripheral).
 */
#define USE_HAL_DCMIPP_REGISTER_CALLBACKS 0U    /* DCMIPP register callback disabled    */
#define USE_HAL_I2C_REGISTER_CALLBACKS 0U       /* I2C register callback disabled       */
#define USE_HAL_RAMCFG_REGISTER_CALLBACKS 0U    /* RAMCFG register callback disabled    */
#define USE_HAL_SD_REGISTER_CALLBACKS 0U        /* SD register callback disabled        */
#define USE_HAL_TIM_REGISTER_CALLBACKS 0U       /* TIM register callback disabled       */
#define USE_HAL_UART_REGISTER_CALLBACKS 0U      /* UART register callback disabled      */

/* ################## SDMMC peripheral configuration ######################### */

#define USE_SD_TRANSCEIVER 0U

/* Includes ------------------------------------------------------------------*/
/**
 * @brief Include module's header file
 */
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32n6xx_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32n6xx_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_RIF_MODULE_ENABLED
#include "stm32n6xx_hal_rif.h"
#endif /* HAL_RIF_MODULE_ENABLED */

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32n6xx_hal_dma.h"
#endif /* HAL_DMA_MODULE_ENABLED */

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32n6xx_hal_cortex.h"
#endif /* HAL_CORTEX_MODULE_ENABLED */

#ifdef HAL_BSEC_MODULE_ENABLED
#include "stm32n6xx_hal_bsec.h"
#endif /* HAL_BSEC_MODULE_ENABLED */

#ifdef HAL_DCMIPP_MODULE_ENABLED
#include "stm32n6xx_hal_dcmipp.h"
#endif /* HAL_DCMIPP_MODULE_ENABLED */

#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32n6xx_hal_exti.h"
#endif /* HAL_EXTI_MODULE_ENABLED */

#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32n6xx_hal_i2c.h"
#endif /* HAL_I2C_MODULE_ENABLED */

#ifdef HAL_JPEG_MODULE_ENABLED
#include "stm32n6xx_hal_jpeg.h"
#endif /* HAL_JPEG_MODULE_ENABLED */

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32n6xx_hal_pwr.h"
#endif /* HAL_PWR_MODULE_ENABLED */

#ifdef HAL_RAMCFG_MODULE_ENABLED
#include "stm32n6xx_hal_ramcfg.h"
#endif /* HAL_RAMCFG_MODULE_ENABLED */

#ifdef HAL_SD_MODULE_ENABLED
#include "stm32n6xx_hal_sd.h"
#endif /* HAL_SD_MODULE_ENABLED */

#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32n6xx_hal_tim.h"
#endif /* HAL_TIM_MODULE_ENABLED */

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32n6xx_hal_uart.h"
#endif /* HAL_UART_MODULE_ENABLED */

/* Exported macros -----------------------------------------------------------*/
#ifdef USE_FULL_ASSERT
/**
 * @brief  The assert_param macro is used for function's parameters check.
 * @param  expr: If expr is false, it calls assert_failed function
 *         which reports the name of the source file and the source
 *         line number of the call that failed.
 *         If expr is true, it returns no value.
 * @retval None
 */
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
/* Exported functions ------------------------------------------------------- */
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif /* USE_FULL_ASSERT */

#ifdef __cplusplus
}
#endif

#endif /* STM32N6xx_HAL_CONF_H */
