#include "image_server.h"

#include <string.h>

#include "app.h"
#include "ff.h"
#include "image_protocol.h"

static FIL snapshot_file;
static payload_saved_image_t snapshot;
static int snapshot_open;
static uint8_t cached_response[IMAGE_SPI_FRAME_SIZE];
static uint8_t cached_request[IMAGE_SPI_FRAME_SIZE];
static int cached_valid;

volatile uint32_t debug_image_info_requests;
volatile uint32_t debug_image_chunks;
volatile uint32_t debug_image_bytes;
volatile uint32_t debug_image_errors;

static int close_snapshot(void) {
    int result = 0;
    if (snapshot_open) {
        result = f_close(&snapshot_file) != FR_OK;
        snapshot_open = 0;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    return result;
}

void image_server_init(uint8_t response[IMAGE_SPI_FRAME_SIZE]) {
    close_snapshot();
    cached_valid = 0;
    image_frame_init(response, IMAGE_OP_INFO | IMAGE_RESPONSE_FLAG,
                     IMAGE_STATUS_BUSY, 0, 0, 0, 0, 0);
    image_frame_seal(response);
}

int image_server_handle(const uint8_t request[IMAGE_SPI_FRAME_SIZE],
                         uint8_t response[IMAGE_SPI_FRAME_SIZE]) {
    if (!image_frame_valid(request)) {
        debug_image_errors++;
        return 0;
    }

    const uint8_t command = request[4];
    const uint32_t sequence = image_get_u32(request + 8);
    const uint32_t image_id = image_get_u32(request + 12);
    const uint32_t offset = image_get_u32(request + 16);

    if (command == IMAGE_OP_NOP)
        return 0;

    /* Retries must keep the same file even when a newer capture has finished. */
    if (cached_valid && memcmp(request, cached_request, IMAGE_SPI_FRAME_SIZE) == 0) {
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
        } else if (f_open(&snapshot_file, latest.filename, FA_READ) != FR_OK) {
            status = IMAGE_STATUS_IO_ERROR;
        } else {
            snapshot_open = 1;
            if (f_size(&snapshot_file) != latest.length) {
                close_snapshot();
                status = IMAGE_STATUS_IO_ERROR;
            } else {
                snapshot = latest;
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
    } else {
        status = IMAGE_STATUS_BAD_REQUEST;
    }

    response[5] = status;
    image_put_u16(response + 6, length);
    if (snapshot_open) {
        image_put_u32(response + 12, snapshot.image_id);
        image_put_u32(response + 20, snapshot.length);
        image_put_u32(response + 24, snapshot.crc32);
    }
    if (status != IMAGE_STATUS_OK)
        debug_image_errors++;
    image_frame_seal(response);
    memcpy(cached_request, request, IMAGE_SPI_FRAME_SIZE);
    cached_valid = 1;
    memcpy(cached_response, response, IMAGE_SPI_FRAME_SIZE);
    return 1;
}
