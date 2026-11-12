#include "image_server.h"

#include <string.h>

#include "app.h"
#include "ff.h"
#include "image_protocol.h"
#include "stm32n6xx_hal.h"

static FIL snapshot_file;
static payload_saved_image_t snapshot;
static int snapshot_open;
static uint8_t cached_response[IMAGE_SPI_FRAME_SIZE];
static uint8_t cached_request[IMAGE_SPI_FRAME_SIZE];
static int cached_valid;
static uint32_t last_activity;
static int expired_response_pending;
static uint32_t observed_image_id;
static uint32_t offer_started;
static uint32_t reply_started;
static int awaiting_info;
static int awaiting_reply;

volatile uint32_t debug_image_info_requests;
volatile uint32_t debug_image_chunks;
volatile uint32_t debug_image_bytes;
volatile uint32_t debug_image_errors;
volatile uint32_t debug_image_transfer_active;
volatile uint32_t debug_image_timeouts;
volatile uint32_t debug_image_capture_wait;
volatile uint32_t debug_image_handoff_timeouts;

static void observe_latest_image(void) {
    payload_saved_image_t latest;
    if (payload_get_latest_saved_image(&latest) && latest.image_id != observed_image_id) {
        observed_image_id = latest.image_id;
        offer_started = HAL_GetTick();
        awaiting_info = 1;
    }
}

static void expire_handoff_waits(void) {
    const uint32_t now = HAL_GetTick();
    if (awaiting_info && (uint32_t)(now - offer_started) >= IMAGE_TRANSFER_TIMEOUT_MS) {
        awaiting_info = 0;
        debug_image_handoff_timeouts++;
    }
    if (awaiting_reply && (uint32_t)(now - reply_started) >= IMAGE_TRANSFER_TIMEOUT_MS) {
        awaiting_reply = 0;
        debug_image_handoff_timeouts++;
    }
    debug_image_capture_wait = (awaiting_info ? 1U : 0U) | (awaiting_reply ? 2U : 0U);
}

static int close_snapshot(void) {
    int result = 0;
    if (snapshot_open) {
        result = f_close(&snapshot_file) != FR_OK;
        snapshot_open = 0;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    cached_valid = 0;
    debug_image_transfer_active = 0;
    return result;
}

int image_server_transfer_active(void) {
    if (snapshot_open &&
        (uint32_t)(HAL_GetTick() - last_activity) >= IMAGE_TRANSFER_TIMEOUT_MS) {
        /* Invalidate successful INFO/READ replies as well as the open file.
         * A late READ retry must not revive a completed or abandoned lease. */
        expired_response_pending = cached_valid && cached_response[5] == IMAGE_STATUS_OK &&
            (cached_request[4] == IMAGE_OP_INFO || cached_request[4] == IMAGE_OP_READ);
        if (close_snapshot())
            debug_image_errors++;
        if (expired_response_pending) {
            cached_response[5] = IMAGE_STATUS_STALE;
            image_put_u16(cached_response + 6, 0);
            image_frame_seal(cached_response);
        }
        debug_image_timeouts++;
    }
    return snapshot_open;
}

int image_server_capture_ready(void) {
    observe_latest_image();
    expire_handoff_waits();
    return !image_server_transfer_active() && !awaiting_info && !awaiting_reply;
}

void image_server_init(uint8_t response[IMAGE_SPI_FRAME_SIZE]) {
    close_snapshot();
    cached_valid = 0;
    expired_response_pending = 0;
    observed_image_id = 0;
    awaiting_info = awaiting_reply = 0;
    debug_image_capture_wait = 0;
    image_frame_init(response, IMAGE_OP_INFO | IMAGE_RESPONSE_FLAG,
                     IMAGE_STATUS_BUSY, 0, 0, 0, 0, 0);
    image_frame_seal(response);
}

int image_server_handle(const uint8_t request[IMAGE_SPI_FRAME_SIZE],
                         uint8_t response[IMAGE_SPI_FRAME_SIZE]) {
    /* Observe before INFO can acknowledge a just-published image; otherwise
     * the next main-loop check could incorrectly start another offer wait. */
    observe_latest_image();
    expire_handoff_waits();
    image_server_transfer_active();
    if (!image_frame_valid(request)) {
        debug_image_errors++;
        return 0;
    }

    const uint8_t command = request[4];
    const uint32_t sequence = image_get_u32(request + 8);
    const uint32_t image_id = image_get_u32(request + 12);
    const uint32_t offset = image_get_u32(request + 16);

    /* This complete exchange has clocked out the previously queued reply.
     * Clearing before cached-request handling also lets an exact terminal
     * retry acknowledge delivery without restarting its wait. */
    awaiting_reply = 0;
    debug_image_capture_wait = awaiting_info ? 1U : 0U;

    if (command == IMAGE_OP_NOP) {
        if (expired_response_pending) {
            memcpy(response, cached_response, IMAGE_SPI_FRAME_SIZE);
            expired_response_pending = 0;
            return 1;
        }
        return 0;
    }

    expired_response_pending = 0;

    /* Retries must keep the same file even when a newer capture has finished. */
    if (cached_valid && memcmp(request, cached_request, IMAGE_SPI_FRAME_SIZE) == 0) {
        if (snapshot_open && cached_response[5] == IMAGE_STATUS_OK &&
            (command == IMAGE_OP_INFO || command == IMAGE_OP_READ) &&
            image_get_u32(cached_response + 12) == snapshot.image_id)
            last_activity = HAL_GetTick();
        memcpy(response, cached_response, IMAGE_SPI_FRAME_SIZE);
        return 1;
    }

    uint8_t status = IMAGE_STATUS_OK;
    uint16_t length = 0;
    image_frame_init(response, command | IMAGE_RESPONSE_FLAG, IMAGE_STATUS_OK, sequence,
                     image_id, offset, 0, 0);

    if (image_get_u16(request + 6) != 0 || request[5] != 0) {
        status = IMAGE_STATUS_BAD_REQUEST;
    } else if (command == IMAGE_OP_INFO) {
        payload_saved_image_t latest;
        debug_image_info_requests++;
        if (close_snapshot()) {
            status = IMAGE_STATUS_IO_ERROR;
        } else if (!payload_get_latest_saved_image(&latest)) {
            status = IMAGE_STATUS_NO_IMAGE;
        } else {
            awaiting_info = 0;
            image_put_u32(response + 12, latest.image_id);
            image_put_u32(response + 20, latest.length);
            image_put_u32(response + 24, latest.crc32);
            /* INFO's image_id is the master's last completed image. Polling
             * that same image must leave the camera free to produce another. */
            if (image_id != latest.image_id) {
                if (f_open(&snapshot_file, latest.filename, FA_READ) != FR_OK) {
                    status = IMAGE_STATUS_IO_ERROR;
                } else {
                    snapshot_open = 1;
                    if (f_size(&snapshot_file) != latest.length)
                        status = IMAGE_STATUS_IO_ERROR;
                    else
                        snapshot = latest;
                }
            } else {
                awaiting_reply = 1;
                reply_started = HAL_GetTick();
            }
        }
    } else if (command == IMAGE_OP_READ) {
        if (!snapshot_open || image_id != snapshot.image_id) {
            status = IMAGE_STATUS_STALE;
        } else if (offset >= snapshot.length) {
            status = IMAGE_STATUS_BAD_REQUEST;
        } else {
            UINT received = 0;
            const uint32_t remaining = snapshot.length - offset;
            length = remaining < IMAGE_SPI_DATA_SIZE ? remaining : IMAGE_SPI_DATA_SIZE;
            if (f_lseek(&snapshot_file, offset) != FR_OK ||
                f_read(&snapshot_file, response + IMAGE_SPI_DATA_OFFSET, length, &received) != FR_OK ||
                received != length) {
                status = IMAGE_STATUS_IO_ERROR;
                length = 0;
            } else {
                debug_image_chunks++;
                debug_image_bytes += length;
            }
        }
    } else if (command == IMAGE_OP_RELEASE) {
        if (snapshot_open && image_id && image_id != snapshot.image_id)
            status = IMAGE_STATUS_STALE;
        else if (close_snapshot())
            status = IMAGE_STATUS_IO_ERROR;
        if (status == IMAGE_STATUS_OK) {
            awaiting_reply = 1;
            reply_started = HAL_GetTick();
        }
    } else {
        status = IMAGE_STATUS_BAD_REQUEST;
    }

    if (status != IMAGE_STATUS_OK) {
        close_snapshot();
        debug_image_errors++;
    } else if (snapshot_open && (command == IMAGE_OP_INFO || command == IMAGE_OP_READ)) {
        last_activity = HAL_GetTick();
        debug_image_transfer_active = 1;
    }
    response[5] = status;
    image_put_u16(response + 6, length);
    if (snapshot_open) {
        image_put_u32(response + 12, snapshot.image_id);
        image_put_u32(response + 20, snapshot.length);
        image_put_u32(response + 24, snapshot.crc32);
    }
    image_frame_seal(response);
    memcpy(cached_request, request, IMAGE_SPI_FRAME_SIZE);
    cached_valid = 1;
    memcpy(cached_response, response, IMAGE_SPI_FRAME_SIZE);
    debug_image_capture_wait = (awaiting_info ? 1U : 0U) | (awaiting_reply ? 2U : 0U);
    return 1;
}
