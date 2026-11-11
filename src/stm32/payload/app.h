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

typedef enum {
    PAYLOAD_OK = 0,
    PAYLOAD_BUSY = -1,
    PAYLOAD_TIMEOUT = -2,
    PAYLOAD_CAMERA_ERROR = -3,
    PAYLOAD_JPEG_ERROR = -4,
    PAYLOAD_STORAGE_ERROR = -5,
    PAYLOAD_INVALID_ARGUMENT = -6,
    PAYLOAD_NOT_INITIALIZED = -7,
} payload_status_t;

typedef struct {
    const uint8_t *data;
    uint32_t length;
    uint32_t width;
    uint32_t height;
} payload_image_t;

payload_status_t payload_camera_init(void);
payload_status_t payload_camera_poll(void);
payload_status_t payload_take_jpeg(payload_image_t *image, uint32_t timeout_ms);
payload_status_t payload_save_jpeg(const payload_image_t *image);
void payload_release_image(void);
void app_capture_step(int automatic_capture);

extern volatile int32_t debug_capture_status;

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
extern volatile uint32_t debug_jpeg_sd_verify_status;
extern volatile uint32_t debug_jpeg_sd_verified_bytes;
extern volatile uint32_t debug_jpeg_sd_verified;
extern char debug_jpeg_sd_filename[20];

#endif
