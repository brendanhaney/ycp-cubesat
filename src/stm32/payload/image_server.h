#ifndef PAYLOAD_IMAGE_SERVER_H
#define PAYLOAD_IMAGE_SERVER_H

#include <stdint.h>
#include "image_protocol.h"

/* Called only from the main loop, between capture/SD-save operations. */
void image_server_init(uint8_t response[IMAGE_SPI_FRAME_SIZE]);
/* Returns one for a response, zero for an ignored NOP or invalid frame. */
int image_server_handle(const uint8_t request[IMAGE_SPI_FRAME_SIZE],
                         uint8_t response[IMAGE_SPI_FRAME_SIZE]);

extern volatile uint32_t debug_image_info_requests;
extern volatile uint32_t debug_image_chunks;
extern volatile uint32_t debug_image_bytes;
extern volatile uint32_t debug_image_errors;

#endif
