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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_cam.h"
#include "app_config.h"
#include "app_jpg.h"
#include "cmw_camera.h"
#include "stm32n6xx_hal.h"
#include "ulist.h"
#include "stm32n6xx_nucleo.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "utils.h"
#include "ff.h"

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "dev"
#endif

#define CAPT_BUFFER_NB 2

#define FREERTOS_PRIORITY(p) ((UBaseType_t)((int)tskIDLE_PRIORITY + configMAX_PRIORITIES / 2 + (p)))

struct list_protected {
    struct ulist head;
    SemaphoreHandle_t lock;
    StaticSemaphore_t lock_buffer;
    SemaphoreHandle_t sem;
    StaticSemaphore_t sem_buffer;
    int level_dbg;
};

struct buffer {
    struct ulist list;
    uint8_t *buffer;
    int len;
};

/* threads */
/* camera thread */
static StaticTask_t camera_thread;
static StackType_t camera_thread_stack[2 * configMINIMAL_STACK_SIZE];
/* isp thread */
static StaticTask_t isp_thread;
static StackType_t isp_thread_stack[2 * configMINIMAL_STACK_SIZE];
static SemaphoreHandle_t isp_sem;
static StaticSemaphore_t isp_sem_buffer;
/* capture thread */
static StaticTask_t capture_thread;
static StackType_t capture_thread_stack[configMINIMAL_STACK_SIZE];
static SemaphoreHandle_t capture_sem;
static StaticSemaphore_t capture_sem_buffer;

/* Raw camera frames and encoded JPEG use separate internal SRAM banks. */
static uint8_t capture_buffer[CAPT_BUFFER_NB][MAX_IMG_FRAME_SIZE] ALIGN_32 IN_CAPTURE_RAM;
static struct buffer capture[CAPT_BUFFER_NB];
static uint8_t debug_jpeg_buffer[MAX_IMG_FRAME_SIZE] ALIGN_32
    __attribute__((section(".sram56_bss")));

struct list_protected capt_free_buffers;
struct list_protected capt_capturing_buffers;
struct list_protected capt_ready_buffers;

static int lp_init(struct list_protected *lp) {
    ulist_init_head(&lp->head);
    lp->lock = xSemaphoreCreateMutexStatic(&lp->lock_buffer);
    assert(lp->lock);
    lp->sem = xSemaphoreCreateCountingStatic(CAPT_BUFFER_NB, 0, &lp->sem_buffer);
    assert(lp->sem);
    lp->level_dbg = 0;

    return 0;
}

static void lp_lock(struct list_protected *lp) {
    int ret;

    ret = xSemaphoreTake(lp->lock, portMAX_DELAY);
    assert(ret == pdTRUE);
}

static void lp_unlock(struct list_protected *lp) {
    int ret;

    ret = xSemaphoreGive(lp->lock);
    assert(ret == pdTRUE);
}

static int lp_push(struct list_protected *lp, struct buffer *buffer) {
    int ret;

    assert(!xPortIsInsideInterrupt());

    lp_lock(lp);
    ulist_add_tail(&buffer->list, &lp->head);
    lp->level_dbg++;
    lp_unlock(lp);

    ret = xSemaphoreGive(lp->sem);
    assert(ret == pdTRUE);

    return 0;
}

static struct buffer *lp_pop(struct list_protected *lp, int is_blocking) {
    struct buffer *res;
    int ret;

    assert(!xPortIsInsideInterrupt());

    ret = xSemaphoreTake(lp->sem, is_blocking ? portMAX_DELAY : 0);
    if (ret == pdFALSE)
        return NULL;

    lp_lock(lp);
    res = ulist_entry(lp->head.next, struct buffer, list);
    lp->level_dbg--;
    assert(res);
    ulist_del(&res->list);
    lp_unlock(lp);

    return res;
}

static void app_main_pipe_vsync_event() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    ret = xSemaphoreGiveFromISR(isp_sem, &xHigherPriorityTaskWoken);
    if (ret == pdTRUE)
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

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

static struct buffer *debug_held_buffer = NULL;

static void isp_thread_fct(void *arg) {
    int ret;

    while (1) {
        ret = xSemaphoreTake(isp_sem, portMAX_DELAY);
        assert(ret == pdTRUE);

        CAM_IspUpdate();
        debug_isp_updates++;
    }
}

static void app_main_pipe_frame_event() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    debug_frame_events++; // Count DCMIPP frame events

    ret = xSemaphoreGiveFromISR(capture_sem, &xHigherPriorityTaskWoken);
    if (ret == pdTRUE)
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void capture_thread_fct(void *arg) {
    struct buffer *current_buffer;
    struct buffer *next_buffer;
    int ret;

    while (1) {
        ret = xSemaphoreTake(capture_sem, portMAX_DELAY);
        assert(ret == pdTRUE);

        next_buffer = lp_pop(&capt_free_buffers, 0);
        if (next_buffer) {
            ret = HAL_DCMIPP_PIPE_SetMemoryAddress(CMW_CAMERA_GetDCMIPPHandle(), DCMIPP_PIPE1,
                                                   DCMIPP_MEMORY_ADDRESS_0,
                                                   (uint32_t)next_buffer->buffer);
            assert(ret == HAL_OK);
            current_buffer = lp_pop(&capt_capturing_buffers, 0);
            assert(current_buffer);
            lp_push(&capt_capturing_buffers, next_buffer);
            lp_push(&capt_ready_buffers, current_buffer);
        }
    }
}

static void debug_release_frame(void) {
    int ret;

    if (debug_held_buffer != NULL) {
        ret = lp_push(&capt_free_buffers, debug_held_buffer);
        assert(ret == 0);
        debug_held_buffer = NULL;
    }

    debug_capture_ready = 0;
    debug_capture_addr = 0;
    debug_capture_len = 0;
    debug_capture_release = 0;
}

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

    /* Find the first available image filename */
    for (image_num = 1; image_num <= 999; image_num++) {
        snprintf(debug_jpeg_sd_filename, sizeof(debug_jpeg_sd_filename), "0:/IMAGE%03lu.JPG",
                 (unsigned long)image_num);

        result = f_open(&debug_jpeg_file, debug_jpeg_sd_filename, FA_CREATE_NEW | FA_WRITE);

        /* Existing image: try the next number */
        if (result == FR_EXIST)
            continue;

        debug_jpeg_sd_open = result;

        if (result != FR_OK) {
            debug_jpeg_sd_done = 1;
            return;
        }

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
    }

    debug_jpeg_sd_done = 1;
}

static void standalone_camera_init(void) {
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
    struct buffer *buffer;
    int ret;

    ret = JPG_Init(&jpg_conf);
    assert(ret == 0);
    CAM_Init(&camera_conf);

    for (int i = 0; i < CAPT_BUFFER_NB; i++)
        capture[i].len = MAX_IMG_FRAME_SIZE;
    buffer = lp_pop(&capt_free_buffers, 1);
    assert(buffer);
    ret = lp_push(&capt_capturing_buffers, buffer);
    assert(ret == 0);
    CAM_CapturePipe_Start(buffer->buffer, CMW_MODE_CONTINUOUS);
}

static void standalone_camera_step(void) {
    struct buffer *buffer;
    int ret;

    /* Return the previously preserved buffer when requested */
    if (debug_capture_release) {
        debug_release_frame();
    }

    /* Retrieve a completed frame */
    buffer = lp_pop(&capt_ready_buffers, 0);

    if (!buffer) {
        HAL_Delay(1);
        return;
    }

    /* Preserve a frame when commanded */
    if (debug_capture_request && !debug_capture_ready) {

        /* Preserve the captured raw frame */
        debug_held_buffer = buffer;
        debug_capture_addr = (uint32_t)buffer->buffer;
        debug_capture_len = buffer->len;

        /* Encode the preserved frame using STM32 JPEG hardware */
        debug_jpeg_ready = 0;
        debug_jpeg_len = 0;

        ret = JPG_Encode(debug_jpeg_buffer, buffer->buffer, sizeof(debug_jpeg_buffer), buffer->len);

        assert(ret > 0 && ret <= (int)sizeof(debug_jpeg_buffer));

        debug_jpeg_addr = (uint32_t)debug_jpeg_buffer;
        debug_jpeg_len = ret;
        debug_jpeg_ready = 1;

        /* Save compressed JPEG to microSD */
        JPEG_SaveToSD();

        /* Flash the green LED once for each captured image. */
        BSP_LED_On(LED_GREEN);
        HAL_Delay(100);
        BSP_LED_Off(LED_GREEN);

        /* Publish the JPEG and SD results before the debugger sees ready. */
        __DMB();
        debug_capture_ready = 1;
        debug_capture_request = 0;

    } else {

        /* Return unwanted frames for reuse */
        ret = lp_push(&capt_free_buffers, buffer);
        assert(ret == 0);
    }
}

static void standalone_camera_thread_fct(void *arg) {
    (void)arg;
    standalone_camera_init();

    const TickType_t capture_interval = pdMS_TO_TICKS(5000);
    TickType_t last_capture = xTaskGetTickCount();
    int automatic_capture = 0;

    while (1) {
        const TickType_t now = xTaskGetTickCount();

        if (!debug_capture_request && !debug_capture_ready &&
            (TickType_t)(now - last_capture) >= capture_interval) {
            last_capture = now;
            automatic_capture = 1;
            debug_capture_request = 1;
        }

        standalone_camera_step();

        /* Return automatic captures after encoding and saving to microSD. */
        if (automatic_capture && debug_capture_ready) {
            debug_release_frame();
            automatic_capture = 0;
        }
    }
}

static void LIST_Init(void) {
    int ret;

    ret = lp_init(&capt_free_buffers);
    assert(ret == 0);
    ret = lp_init(&capt_capturing_buffers);
    assert(ret == 0);
    ret = lp_init(&capt_ready_buffers);
    assert(ret == 0);

    for (int i = 0; i < CAPT_BUFFER_NB; i++) {
        capture[i].buffer = capture_buffer[i];
        ret = lp_push(&capt_free_buffers, &capture[i]);
        assert(ret == 0);
    }
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
    printf("HAL: %lu.%lu.%lu\n", __STM32N6xx_HAL_VERSION_MAIN, __STM32N6xx_HAL_VERSION_SUB1,
           __STM32N6xx_HAL_VERSION_SUB2);
    printf("========================================\n");
}

void app_run(void) {
    UBaseType_t capture_priority = FREERTOS_PRIORITY(3);
    UBaseType_t isp_priority = FREERTOS_PRIORITY(2);
    UBaseType_t camera_priority = FREERTOS_PRIORITY(0);
    TaskHandle_t hdl;

    app_display_info_header();
    /* Enable DWT so DWT_CYCCNT works when debugger is not attached. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    LIST_Init();
    isp_sem = xSemaphoreCreateCountingStatic(1, 0, &isp_sem_buffer);
    assert(isp_sem);
    capture_sem = xSemaphoreCreateCountingStatic(1, 0, &capture_sem_buffer);
    assert(capture_sem);

    hdl = xTaskCreateStatic(standalone_camera_thread_fct, "camera", configMINIMAL_STACK_SIZE * 2,
                            NULL, camera_priority, camera_thread_stack, &camera_thread);
    assert(hdl != NULL);
    hdl = xTaskCreateStatic(isp_thread_fct, "isp", configMINIMAL_STACK_SIZE * 2, NULL, isp_priority,
                            isp_thread_stack, &isp_thread);
    assert(hdl != NULL);
    hdl = xTaskCreateStatic(capture_thread_fct, "capture", configMINIMAL_STACK_SIZE, NULL,
                            capture_priority, capture_thread_stack, &capture_thread);
    assert(hdl != NULL);

    BSP_LED_Off(LED_GREEN);
}

int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe) {
    if (pipe == DCMIPP_PIPE1)
        app_main_pipe_frame_event();

    return HAL_OK;
}

int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe) {
    if (pipe == DCMIPP_PIPE1)
        app_main_pipe_vsync_event();

    return HAL_OK;
}
