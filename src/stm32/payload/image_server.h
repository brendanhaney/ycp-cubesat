#ifndef PAYLOAD_IMAGE_SERVER_H
#define PAYLOAD_IMAGE_SERVER_H

#include <stdint.h>
#include "config.h"
#include "image_protocol.h"

/* Serve the saved JPEG directly from its retained RAM buffer. Called only
 * from the main loop, between capture/SD-save operations; no filesystem I/O. */
void image_server_init(uint8_t response[IMAGE_SPI_FRAME_SIZE]);

/* Returns one for a response, zero for an ignored NOP or invalid frame.
 * The first NOP after expiry publishes STALE instead of the old success. */
int image_server_handle(const uint8_t request[IMAGE_SPI_FRAME_SIZE], uint8_t response[IMAGE_SPI_FRAME_SIZE]);

/* A new-image INFO holds off captures until release or the configured
 * inactivity timeout. Calling this also expires an abandoned snapshot. */
int image_server_transfer_active(void);

/* Offer each new image for IMAGE_OFFER_WINDOW_MS before capturing again if no
 * INFO arrives. Active transfers and undelivered terminal replies retain
 * the IMAGE_TRANSFER_TIMEOUT_MS inactivity watchdog. */
int image_server_capture_ready(void);

extern volatile uint32_t debug_image_info_requests;
extern volatile uint32_t debug_image_chunks;
extern volatile uint32_t debug_image_bytes;
extern volatile uint32_t debug_image_errors;
extern volatile uint32_t debug_image_transfer_active;
extern volatile uint32_t debug_image_timeouts;

/* Bit 0: waiting for INFO; bit 1: waiting for a terminal reply exchange. */
extern volatile uint32_t debug_image_capture_wait;
extern volatile uint32_t debug_image_handoff_timeouts;

#endif
