#ifndef COMMS_BUFFERS_H
#define COMMS_BUFFERS_H

#include <stdint.h>
#include <image_protocol.h>
#include <telemetry_protocol.h>

#define COMMS_BUFFER_READY (1U << 0)
#define COMMS_BUFFER_UPDATED (1U << 1)
#define COMMS_BUFFER_RECEIVING (1U << 2)
#define COMMS_BUFFER_HANDLING (1U << 3)
#define COMMS_BUFFER_ERROR (1U << 4)
#define COMMS_IMAGE_HEAD_SIZE (480U * 1024U)

struct comms_image_buffer {
    volatile uint32_t flags;
    uint32_t image_id, length, crc32, received;
};
struct comms_telemetry_buffer {
    volatile uint32_t flags;
    uint32_t length, crc32;
    uint32_t words[TELEMETRY_WORDS];
};

extern struct comms_image_buffer comms_image_buffer;
extern struct comms_telemetry_buffer comms_telemetry_buffer;

void comms_buffers_init(void);
int comms_image_begin(uint32_t image_id, uint32_t length, uint32_t crc32);
int comms_image_append(uint32_t offset, const uint8_t *data, uint32_t count);
int comms_image_complete(void);
const uint8_t *comms_image_data(uint32_t offset, uint32_t *contiguous);
int comms_telemetry_begin(void);
int comms_telemetry_complete(const uint8_t *data, uint32_t length, uint32_t crc32);
int comms_buffer_acquire(volatile uint32_t *flags);
void comms_buffer_release(volatile uint32_t *flags);
void comms_buffer_abort(volatile uint32_t *flags);

#endif
