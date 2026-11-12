/**
******************************************************************************
* @file    app.c
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

#include "app.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_cam.h"
#include "app_config.h"
#include "app_jpg.h"
#include "cmw_camera.h"
#include "stm32n6xx_hal.h"
#include "stm32n6xx_nucleo.h"
#include "utils.h"
#include "ff.h"
#include "image_protocol.h"

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "dev"
#endif

#define CAPT_BUFFER_NB 2

static uint8_t capture_buffer[CAPT_BUFFER_NB][MAX_IMG_FRAME_SIZE] ALIGN_32 IN_CAPTURE_RAM;
static uint8_t debug_jpeg_buffer[MAX_IMG_FRAME_SIZE] ALIGN_32
    __attribute__((section(".sram56_bss")));

static volatile uint32_t isp_pending;
static volatile uint32_t capture_armed;
static volatile uint32_t camera_error;
static volatile int held_capture = -1;
static uint32_t active_capture;
static int camera_initialized;
static payload_saved_image_t latest_saved_image;
static uint32_t next_image_num = 1;

volatile uint32_t debug_frame_events = 0;
volatile uint32_t debug_isp_updates = 0;
volatile uint32_t debug_capture_request = 0;
volatile uint32_t debug_capture_ready = 0;
volatile uint32_t debug_capture_addr = 0;
volatile uint32_t debug_capture_len = 0;
volatile uint32_t debug_capture_release = 0;

volatile uint32_t debug_jpeg_ready = 0;
volatile uint32_t debug_jpeg_addr = 0;
volatile uint32_t debug_jpeg_len = 0;

/* JPEG-to-microSD debug variables */
static FIL debug_jpeg_file;

volatile uint32_t debug_jpeg_sd_open = 0xFFFFFFFFU;
volatile uint32_t debug_jpeg_sd_write = 0xFFFFFFFFU;
volatile uint32_t debug_jpeg_sd_close = 0xFFFFFFFFU;
volatile uint32_t debug_jpeg_sd_bytes = 0;
volatile uint32_t debug_jpeg_sd_done = 0;
volatile uint32_t debug_jpeg_sd_index = 0;
volatile uint32_t debug_jpeg_sd_saved_count = 0;
volatile uint32_t debug_jpeg_sd_no_slot = 0;
volatile uint32_t debug_jpeg_sd_verify_status = 0xFFFFFFFFU;
volatile uint32_t debug_jpeg_sd_verified_bytes = 0;
volatile uint32_t debug_jpeg_sd_verified = 0;

char debug_jpeg_sd_filename[20];

volatile int32_t debug_capture_status;

static void JPEG_VerifyOnSD(void) {
    FIL file;
    uint8_t data[512];
    UINT bytes_read;
    uint32_t offset = 0;
    int matches;

    debug_jpeg_sd_verify_status = f_open(&file, debug_jpeg_sd_filename, FA_READ);
    if (debug_jpeg_sd_verify_status != FR_OK)
        return;

    matches = f_size(&file) == debug_jpeg_len;
    while (matches && offset < debug_jpeg_len) {
        const UINT remaining = debug_jpeg_len - offset;
        const UINT count = remaining < sizeof(data) ? remaining : sizeof(data);

        debug_jpeg_sd_verify_status = f_read(&file, data, count, &bytes_read);
        if (debug_jpeg_sd_verify_status != FR_OK || bytes_read != count) {
            matches = 0;
            break;
        }

        debug_jpeg_sd_verified_bytes += bytes_read;
        matches = memcmp(data, debug_jpeg_buffer + offset, count) == 0;
        offset += bytes_read;
    }

    const FRESULT close_status = f_close(&file);
    if (debug_jpeg_sd_verify_status == FR_OK)
        debug_jpeg_sd_verify_status = close_status;

    debug_jpeg_sd_verified = matches && offset == debug_jpeg_len && close_status == FR_OK;
}

static void JPEG_SaveToSD(void) {
    UINT bytes_written = 0;
    uint32_t image_num;
    FRESULT result;

    /* Reset results for this capture */
    debug_jpeg_sd_done = 0;
    debug_jpeg_sd_open = 0xFFFFFFFFU;
    debug_jpeg_sd_write = 0xFFFFFFFFU;
    debug_jpeg_sd_close = 0xFFFFFFFFU;
    debug_jpeg_sd_bytes = 0;
    debug_jpeg_sd_index = 0;
    debug_jpeg_sd_no_slot = 0;
    debug_jpeg_sd_verify_status = 0xFFFFFFFFU;
    debug_jpeg_sd_verified_bytes = 0;
    debug_jpeg_sd_verified = 0;

    /* Verify that the JPEG length is valid */
    if (debug_jpeg_len == 0 || debug_jpeg_len > sizeof(debug_jpeg_buffer)) {
        debug_jpeg_sd_done = 1;
        return;
    }

    /* Scan existing names once per boot, then continue from the next number.
     * CREATE_NEW still protects files added by another writer. Never wrap. */
    for (image_num = next_image_num; image_num <= 999; image_num++) {
        snprintf(debug_jpeg_sd_filename, sizeof(debug_jpeg_sd_filename), "0:/IMAGE%03lu.JPG",
                 (unsigned long)image_num);

        result = f_open(&debug_jpeg_file, debug_jpeg_sd_filename, FA_CREATE_NEW | FA_WRITE);

        /* Existing image: try the next number */
        if (result == FR_EXIST) {
            next_image_num = image_num + 1;
            continue;
        }

        debug_jpeg_sd_open = result;

        if (result != FR_OK) {
            debug_jpeg_sd_done = 1;
            return;
        }

        /* Creation consumes the name even if writing or verification fails. */
        next_image_num = image_num + 1;
        debug_jpeg_sd_index = image_num;
        break;
    }

    /* All 999 filenames are occupied */
    if (image_num > 999) {
        debug_jpeg_sd_no_slot = 1;
        debug_jpeg_sd_open = FR_EXIST;
        debug_jpeg_sd_done = 1;
        return;
    }

    /* Write the compressed JPEG */
    debug_jpeg_sd_write =
        f_write(&debug_jpeg_file, debug_jpeg_buffer, (UINT)debug_jpeg_len, &bytes_written);

    debug_jpeg_sd_bytes = bytes_written;

    /* Commit file and filesystem metadata */
    debug_jpeg_sd_close = f_close(&debug_jpeg_file);

    /* Count only fully successful saves */
    if (debug_jpeg_sd_write == FR_OK && bytes_written == debug_jpeg_len &&
        debug_jpeg_sd_close == FR_OK) {
        debug_jpeg_sd_saved_count++;
        JPEG_VerifyOnSD();
        if (debug_jpeg_sd_verified) {
            memcpy(latest_saved_image.filename, debug_jpeg_sd_filename,
                   sizeof(latest_saved_image.filename));
            latest_saved_image.image_id = debug_jpeg_sd_index;
            latest_saved_image.length = debug_jpeg_len;
            latest_saved_image.crc32 = image_crc32(debug_jpeg_buffer, debug_jpeg_len);
        }
    }

    debug_jpeg_sd_done = 1;
}

static void app_display_info_header() {
    printf("========================================\n");
    printf("CubeSat payload (%s)\n", APP_VERSION_STRING);
    printf("Build date & time: %s %s\n", __DATE__, __TIME__);
#if defined(__GNUC__)
    printf("Compiler: GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__ICCARM__)
    printf("Compiler: IAR EWARM %d.%d.%d\n", __VER__ / 1000000, (__VER__ / 1000) % 1000,
           __VER__ % 1000);
#else
    printf("Compiler: Unknown\n");
#endif
    printf("HAL: %lu.%lu.%lu\n", __STM32N6xx_HAL_VERSION_MAIN, __STM32N6xx_HAL_VERSION_SUB1, __STM32N6xx_HAL_VERSION_SUB2);
    printf("========================================\n");
}

payload_status_t payload_camera_init(void) {

    CAM_conf_t camera_conf = {
        .capture_width = CAPTURE_WIDTH,
        .capture_height = CAPTURE_HEIGHT,
        .fps = CAPTURE_FPS,
        .dcmipp_output_format = DCMIPP_PIXEL_PACKER_FORMAT_YUV422_1,
        .is_rgb_swap = 0,
    };

    JPG_conf_t jpg_conf = {
        .width = CAPTURE_WIDTH,
        .height = CAPTURE_HEIGHT,
        .fmt_src = JPG_SRC_YUV422,
    };

    if (camera_initialized)
        return PAYLOAD_BUSY;

    app_display_info_header();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    if (JPG_Init(&jpg_conf) != 0)
        return PAYLOAD_JPEG_ERROR;

    if (CAM_Init(&camera_conf) != CMW_ERROR_NONE) {

        JPG_Deinit();

        return PAYLOAD_CAMERA_ERROR;
    }

    isp_pending = 0;
    capture_armed = 0;
    camera_error = 0;
    held_capture = -1;
    active_capture = 0;

    if (CAM_CapturePipe_Start(capture_buffer[0], CMW_MODE_CONTINUOUS) != CMW_ERROR_NONE) {

        CAM_Deinit();

        JPG_Deinit();

        return PAYLOAD_CAMERA_ERROR;
    }

    camera_initialized = 1;

    BSP_LED_Off(LED_GREEN);

    return PAYLOAD_OK;
}

payload_status_t payload_camera_poll(void) {

    if (!camera_initialized)
        return PAYLOAD_NOT_INITIALIZED;

    if (camera_error)
        return PAYLOAD_CAMERA_ERROR;

    const uint32_t primask = __get_PRIMASK();

    __disable_irq();

    const uint32_t pending = isp_pending;

    isp_pending = 0;

    __set_PRIMASK(primask);

    if (pending) {

        if (CAM_IspUpdate() != CMW_ERROR_NONE) {

            camera_error = 1;

            return PAYLOAD_CAMERA_ERROR;
        }

        debug_isp_updates++;
    }

    return PAYLOAD_OK;
}

void payload_release_image(void) {

    const uint32_t primask = __get_PRIMASK();

    __disable_irq();

    capture_armed = 0;
    held_capture = -1;

    __set_PRIMASK(primask);

    debug_capture_ready = 0;
    debug_capture_addr = 0;
    debug_capture_len = 0;
    debug_capture_release = 0;
    debug_jpeg_ready = 0;
}

payload_status_t payload_take_jpeg(payload_image_t *image, uint32_t timeout_ms) {

    if (image == NULL || __get_IPSR() != 0 || __get_PRIMASK() != 0)
        return PAYLOAD_INVALID_ARGUMENT;

    *image = (payload_image_t){0};

    if (!camera_initialized)
        return PAYLOAD_NOT_INITIALIZED;

    if (held_capture >= 0 || capture_armed)
        return PAYLOAD_BUSY;

    if (camera_error)
        return PAYLOAD_CAMERA_ERROR;

    const uint32_t start = HAL_GetTick();
    const uint32_t updates = debug_isp_updates;

    while ((uint32_t)(debug_isp_updates - updates) < CAPTURE_SETTLE_FRAMES) {

        const payload_status_t status = payload_camera_poll();

        if (status != PAYLOAD_OK)
            return status;

        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
            return PAYLOAD_TIMEOUT;

        __WFI();
    }

    capture_armed = 1;

    while (held_capture < 0 && !camera_error) {

        const payload_status_t status = payload_camera_poll();

        if (status != PAYLOAD_OK)
            break;

        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
            break;

        __WFI();
    }

    const uint32_t primask = __get_PRIMASK();

    __disable_irq();

    capture_armed = 0;

    const int captured = held_capture;

    __set_PRIMASK(primask);

    if (camera_error) {
        payload_release_image();
        return PAYLOAD_CAMERA_ERROR;
    }

    if (captured < 0)
        return PAYLOAD_TIMEOUT;

    __DMB();

    debug_capture_addr = (uint32_t)capture_buffer[captured];
    debug_capture_len = MAX_IMG_FRAME_SIZE;
    debug_jpeg_ready = 0;
    debug_jpeg_len = 0;

    const int length = JPG_Encode(debug_jpeg_buffer, capture_buffer[captured], sizeof(debug_jpeg_buffer), MAX_IMG_FRAME_SIZE);

    if (length <= 0 || length > (int)sizeof(debug_jpeg_buffer)) {
        payload_release_image();
        return PAYLOAD_JPEG_ERROR;
    }

    image->data = debug_jpeg_buffer;
    image->length = length;
    image->width = CAPTURE_WIDTH;
    image->height = CAPTURE_HEIGHT;

    debug_jpeg_addr = (uint32_t)debug_jpeg_buffer;
    debug_jpeg_len = length;

    __DMB();

    debug_jpeg_ready = 1;

    return PAYLOAD_OK;
}

payload_status_t payload_save_jpeg(const payload_image_t *image) {

    if (image == NULL || image->data != debug_jpeg_buffer || !debug_jpeg_ready ||
        image->length != debug_jpeg_len || image->length == 0 || held_capture < 0)
        return PAYLOAD_INVALID_ARGUMENT;

    JPEG_SaveToSD();

    if (!debug_jpeg_sd_verified || debug_jpeg_sd_verify_status != FR_OK)
        return PAYLOAD_STORAGE_ERROR;

    return PAYLOAD_OK;
}

int payload_get_latest_saved_image(payload_saved_image_t *image) {
    if (image == NULL || latest_saved_image.length == 0)
        return 0;
    *image = latest_saved_image;
    return 1;
}

void app_capture_step(int automatic_capture) {

    if (debug_capture_release)
        payload_release_image();

    if ((debug_capture_request || automatic_capture) && !debug_capture_ready) {

        payload_image_t image;

        debug_capture_status = payload_take_jpeg(&image, CAPTURE_TIMEOUT_MS);

        if (debug_capture_status == PAYLOAD_OK)
            debug_capture_status = payload_save_jpeg(&image);

        BSP_LED_On(LED_GREEN);

        HAL_Delay(100);

        BSP_LED_Off(LED_GREEN);

        if (debug_capture_request) {

            __DMB();

            debug_capture_ready = 1;
            debug_capture_request = 0;

        } else {

            payload_release_image();

        }
    }

    payload_camera_poll();
}

int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe) {

    if (pipe != DCMIPP_PIPE1)
        return HAL_OK;

    debug_frame_events++;

    if (capture_armed && held_capture < 0) {

        const uint32_t next = active_capture ^ 1U;

        if (HAL_DCMIPP_PIPE_SetMemoryAddress(CMW_CAMERA_GetDCMIPPHandle(), DCMIPP_PIPE1, DCMIPP_MEMORY_ADDRESS_0, (uint32_t)capture_buffer[next]) != HAL_OK) {

            camera_error = 1;

            capture_armed = 0;

            return HAL_ERROR;
        }

        __DMB();

        held_capture = active_capture;

        active_capture = next;

        capture_armed = 0;

    }

    return HAL_OK;
}

int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe) {

    if (pipe == DCMIPP_PIPE1)
        isp_pending = 1;

    return HAL_OK;
}

void CMW_CAMERA_PIPE_ErrorCallback(uint32_t pipe) {

    (void)pipe;

    camera_error = 1;
}
