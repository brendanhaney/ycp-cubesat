/**
******************************************************************************
* @file    app.h
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

#ifndef APP_H
#define APP_H

#include <stdint.h>

#define USE_DCACHE

void app_run(void);

/* Set request to retain the next completed frame. Ready is set after JPEG
 * encoding and the SD save attempt. Set release before requesting again.
 * All addresses are target addresses; inspect save diagnostics for SD errors. */
extern volatile uint32_t debug_capture_request;
extern volatile uint32_t debug_capture_ready;
extern volatile uint32_t debug_capture_addr;
extern volatile uint32_t debug_capture_len;
extern volatile uint32_t debug_capture_release;
extern volatile uint32_t debug_frame_events;
extern volatile uint32_t debug_isp_updates;
extern volatile uint32_t debug_jpeg_ready;
extern volatile uint32_t debug_jpeg_addr;
extern volatile uint32_t debug_jpeg_len;
extern volatile uint32_t debug_jpeg_sd_open;
extern volatile uint32_t debug_jpeg_sd_write;
extern volatile uint32_t debug_jpeg_sd_close;
extern volatile uint32_t debug_jpeg_sd_bytes;
extern volatile uint32_t debug_jpeg_sd_done;
extern volatile uint32_t debug_jpeg_sd_index;
extern volatile uint32_t debug_jpeg_sd_saved_count;
extern volatile uint32_t debug_jpeg_sd_no_slot;
extern char debug_jpeg_sd_filename[20];

#endif