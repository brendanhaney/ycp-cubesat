 /**
 ******************************************************************************
 * @file    cmw_io.h
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CMW_IO_H
#define CMW_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#define CAMERA_IMX335_ADDRESS 0x34U

/* Nucleo camera connector: PO5 reset, PA0 enable, I2C2 control bus. */
#define NRST_CAM_PIN GPIO_PIN_5
#define NRST_CAM_PORT GPIOO
#define NRST_CAM_GPIO_ENABLE_VDDIO() HAL_PWREx_EnableVddIO2()
#define NRST_CAM_GPIO_CLK_ENABLE() __HAL_RCC_GPIOO_CLK_ENABLE()
#define EN_CAM_PIN GPIO_PIN_0
#define EN_CAM_PORT GPIOA
#define EN_CAM_GPIO_ENABLE_VDDIO()
#define EN_CAM_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define CMW_I2C_INIT BSP_I2C2_Init
#define CMW_I2C_DEINIT BSP_I2C2_DeInit
#define CMW_I2C_READREG16 BSP_I2C2_ReadReg16
#define CMW_I2C_WRITEREG16 BSP_I2C2_WriteReg16

#ifdef __cplusplus
}
#endif

#endif /* __CMW_IO_H */
