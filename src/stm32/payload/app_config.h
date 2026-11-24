/**
 ******************************************************************************
 * @file    app_config.h
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
#ifndef APP_CONFIG
#define APP_CONFIG

#define CAPTURE_WIDTH 800
#define CAPTURE_HEIGHT 480
#define CAPTURE_FPS 30
#define CAPTURE_SETTLE_FRAMES 30U
#define CAPTURE_TIMEOUT_MS 5000U
#define MAX_IMG_FRAME_SIZE (CAPTURE_WIDTH * CAPTURE_HEIGHT * 2)

#endif
