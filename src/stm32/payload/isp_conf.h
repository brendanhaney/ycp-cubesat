/**
 ******************************************************************************
 * @file    isp_conf.h
 * @author  AIS Application Team
 * @brief   Header configuration file - template
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
#ifndef __ISP_CONF_H
#define __ISP_CONF_H

/* Includes ------------------------------------------------------------------*/
#include "config.h"

#if PAYLOAD_AUTO_EXPOSURE
#define ISP_MW_SW_AEC_ALGO_SUPPORT
#endif
#if PAYLOAD_AUTO_WHITE_BALANCE
#define ISP_MW_SW_AWB_ALGO_SUPPORT
#endif

#endif /* __ISP_CONF_H */
