#include <comms_buffers.h>
#include <stddef.h>
#include <string.h>

struct comms_image_buffer comms_image_buffer;
struct comms_telemetry_buffer comms_telemetry_buffer;
static uint8_t image_head[COMMS_IMAGE_HEAD_SIZE];
#if defined(CUBESAT_STM32H7)
__attribute__((section(".image_buffer_d2"), aligned(32)))
#endif
static uint32_t image_tail[(IMAGE_MAX_JPEG_SIZE - COMMS_IMAGE_HEAD_SIZE) / 4U];

static void barrier(void) {
#if defined(__arm__) || defined(__thumb__)
    __asm__ volatile("dmb" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

void comms_buffers_init(void) {
#if defined(CUBESAT_STM32H7)
    volatile uint32_t *ahb2enr = (volatile uint32_t *)0x580244dcU;
    *ahb2enr |= 7U << 29;
    (void)*ahb2enr;
    barrier();
    for (uint32_t i = 0; i < sizeof(image_tail) / sizeof(image_tail[0]); ++i)
        ((volatile uint32_t *)image_tail)[i] = 0;
    __asm__ volatile("dsb" ::: "memory");
#endif
    memset(&comms_image_buffer, 0, sizeof(comms_image_buffer));
    memset(&comms_telemetry_buffer, 0, sizeof(comms_telemetry_buffer));
    barrier();
    comms_image_buffer.flags = COMMS_BUFFER_READY;
    comms_telemetry_buffer.flags = COMMS_BUFFER_READY;
}

static int begin(volatile uint32_t *flags) {
    if ((*flags & ~COMMS_BUFFER_ERROR) != COMMS_BUFFER_READY)
        return -1;
    *flags = COMMS_BUFFER_RECEIVING;
    barrier();
    return 0;
}

void comms_buffer_abort(volatile uint32_t *flags) {
    barrier();
    *flags = COMMS_BUFFER_READY | COMMS_BUFFER_ERROR;
}

int comms_image_begin(uint32_t image_id, uint32_t length, uint32_t crc32) {
    if (begin(&comms_image_buffer.flags) < 0)
        return -1;
    if (!length || length > IMAGE_MAX_JPEG_SIZE) {
        comms_buffer_abort(&comms_image_buffer.flags);
        return -1;
    }
    comms_image_buffer.image_id = image_id;
    comms_image_buffer.length = length;
    comms_image_buffer.crc32 = crc32;
    comms_image_buffer.received = 0;
    return 0;
}

int comms_image_append(uint32_t offset, const uint8_t *data, uint32_t count) {
    if (comms_image_buffer.flags != COMMS_BUFFER_RECEIVING)
        return -1;
    if (!data || offset != comms_image_buffer.received ||
        offset > comms_image_buffer.length ||
        count > comms_image_buffer.length - offset) {
        comms_buffer_abort(&comms_image_buffer.flags);
        return -1;
    }
    uint32_t head_count = offset < COMMS_IMAGE_HEAD_SIZE ?
                          COMMS_IMAGE_HEAD_SIZE - offset : 0;
    if (head_count > count)
        head_count = count;
    if (head_count)
        memcpy(image_head + offset, data, head_count);
    if (count > head_count)
        memcpy((uint8_t *)image_tail + (offset + head_count - COMMS_IMAGE_HEAD_SIZE),
               data + head_count, count - head_count);
    comms_image_buffer.received += count;
    return 0;
}

int comms_image_complete(void) {
    if (comms_image_buffer.flags != COMMS_BUFFER_RECEIVING ||
        comms_image_buffer.received != comms_image_buffer.length)
        return -1;
    barrier();
    comms_image_buffer.flags = COMMS_BUFFER_UPDATED;
    return 0;
}

const uint8_t *comms_image_data(uint32_t offset, uint32_t *contiguous) {
    if (contiguous)
        *contiguous = 0;
    if (!contiguous || comms_image_buffer.flags != COMMS_BUFFER_HANDLING ||
        offset >= comms_image_buffer.length)
        return NULL;
    uint32_t size = offset < COMMS_IMAGE_HEAD_SIZE ?
                    COMMS_IMAGE_HEAD_SIZE - offset : IMAGE_MAX_JPEG_SIZE - offset;
    if (size > comms_image_buffer.length - offset)
        size = comms_image_buffer.length - offset;
    *contiguous = size;
    return offset < COMMS_IMAGE_HEAD_SIZE ? image_head + offset :
           (const uint8_t *)image_tail + (offset - COMMS_IMAGE_HEAD_SIZE);
}

int comms_telemetry_begin(void) {
    return begin(&comms_telemetry_buffer.flags);
}

int comms_telemetry_complete(const uint8_t *data, uint32_t length, uint32_t crc32) {
    if (comms_telemetry_buffer.flags != COMMS_BUFFER_RECEIVING)
        return -1;
    if (!data || length != TELEMETRY_DATA_SIZE) {
        comms_buffer_abort(&comms_telemetry_buffer.flags);
        return -1;
    }
    for (uint32_t i = 0; i < TELEMETRY_WORDS; ++i)
        comms_telemetry_buffer.words[i] = image_get_u32(data + i * 4U);
    comms_telemetry_buffer.length = length;
    comms_telemetry_buffer.crc32 = crc32;
    barrier();
    comms_telemetry_buffer.flags = COMMS_BUFFER_UPDATED;
    return 0;
}

int comms_buffer_acquire(volatile uint32_t *flags) {
    if (*flags != COMMS_BUFFER_UPDATED)
        return 0;
    *flags = COMMS_BUFFER_HANDLING;
    barrier();
    return 1;
}

void comms_buffer_release(volatile uint32_t *flags) {
    if (*flags == COMMS_BUFFER_HANDLING) {
        barrier();
        *flags = COMMS_BUFFER_READY;
    }
}
