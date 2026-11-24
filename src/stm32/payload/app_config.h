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

#include "config.h"

#if PAYLOAD_UART_BAUD == 0
#error "Payload UART baud must be nonzero"
#endif

/* Reject settings that the current converter, sensor and SRAM layout cannot support. */
#if CAPTURE_WIDTH < 16 || CAPTURE_WIDTH % 16 || CAPTURE_HEIGHT < 8 || CAPTURE_HEIGHT % 8 || \
    CAPTURE_WIDTH < CAPTURE_HEIGHT || CAPTURE_WIDTH > 2592 || CAPTURE_HEIGHT > 1944
#error "Capture size must be landscape, within 2592x1944, width multiple of 16 and height multiple of 8"
#endif
#if MAX_IMG_FRAME_SIZE > 768000U
#error "Capture/JPEG buffers exceed the current comms SRAM capacity (768000 bytes)"
#endif
#if CAPTURE_FPS != 10 && CAPTURE_FPS != 15 && CAPTURE_FPS != 20 && CAPTURE_FPS != 25 && CAPTURE_FPS != 30
#error "IMX335 capture FPS must be 10, 15, 20, 25 or 30"
#endif
#if CAPTURE_WIDTH * 64 <= 2592 && CAPTURE_HEIGHT * 64 <= 1944
#error "Capture size is below the camera scaler's supported range"
#endif
#if CAPTURE_TIMEOUT_MS * 1ULL * CAPTURE_FPS < (CAPTURE_SETTLE_FRAMES + 1ULL) * 1000ULL
#error "Capture timeout must allow the settle frames and one captured frame"
#endif
#if CAPTURE_TIMEOUT_MS < 1 || CAPTURE_TIMEOUT_MS > 0x7fffffffU || \
    PAYLOAD_CAPTURE_INTERVAL_MS > 0x7fffffffU || PAYLOAD_JPEG_TIMEOUT_MS < 1 || \
    PAYLOAD_JPEG_TIMEOUT_MS > 0x7fffffffU
#error "Capture/JPEG timeouts must be positive and intervals fit the timer range"
#endif
#if PAYLOAD_JPEG_QUALITY < 1 || PAYLOAD_JPEG_QUALITY > 100
#error "PAYLOAD_JPEG_QUALITY must be between 1 and 100"
#endif
#if PAYLOAD_CAMERA_MIRROR_FLIP < 0 || PAYLOAD_CAMERA_MIRROR_FLIP > 3
#error "Camera mirror/flip must be 0, 1, 2 or 3"
#endif
#if !((PAYLOAD_SENSOR_WIDTH == 0 && PAYLOAD_SENSOR_HEIGHT == 0) || \
      (PAYLOAD_SENSOR_WIDTH == 2592 && PAYLOAD_SENSOR_HEIGHT == 1944))
#error "IMX335 sensor readout must be native 0/0 or 2592/1944"
#endif
#if (PAYLOAD_AUTO_EXPOSURE != 0 && PAYLOAD_AUTO_EXPOSURE != 1) || \
    (PAYLOAD_AUTO_WHITE_BALANCE != 0 && PAYLOAD_AUTO_WHITE_BALANCE != 1) || \
    (PAYLOAD_STORAGE_STARTUP_TESTS != 0 && PAYLOAD_STORAGE_STARTUP_TESTS != 1)
#error "Payload feature switches must be 0 or 1"
#endif
#if PAYLOAD_FIRST_IMAGE_NUM < 1 || PAYLOAD_MAX_SAVED_IMAGE_NUM < PAYLOAD_FIRST_IMAGE_NUM || \
    PAYLOAD_MAX_SAVED_IMAGE_NUM > 99999
#error "Payload image numbering must fit 1..99999"
#endif
#if PAYLOAD_SD_BUS_WIDTH != 1 && PAYLOAD_SD_BUS_WIDTH != 4
#error "SD bus width must be 1 or 4"
#endif
#if PAYLOAD_SD_CLOCK_DIV < 0 || PAYLOAD_SD_CLOCK_DIV > 1023 || PAYLOAD_SD_IO_TIMEOUT_MS < 1 || \
    PAYLOAD_SD_READY_TIMEOUT_MS < 1 || IMAGE_TRANSFER_TIMEOUT_MS < 1 || \
    PAYLOAD_SD_IO_TIMEOUT_MS > 0x7fffffffU || PAYLOAD_SD_READY_TIMEOUT_MS > 0x7fffffffU || \
    IMAGE_TRANSFER_TIMEOUT_MS > 0x7fffffffU || IMAGE_OFFER_WINDOW_MS > 0x7fffffffU || \
    PAYLOAD_CAPTURE_LED_MS > 0x7fffffffU
#error "SD divider or payload timeout is outside the supported range"
#endif

#endif
