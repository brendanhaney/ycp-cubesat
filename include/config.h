#ifndef CUBESAT_CONFIG_H
#define CUBESAT_CONFIG_H

/* Shared user settings. Rebuild all affected boards after editing this file.
 * Telemetry sensor settings live in sensors.h.
 * Defaults may also be overridden with compiler -D options. Host UART and
 * image tools read the defaults in this header; --baud still overrides them.
 * Peripheral register fields, wire formats, PLLs and SRAM addresses remain
 * hardware/protocol constants in their drivers. */

#ifndef SPI_POLL_LIMIT
#define SPI_POLL_LIMIT 1000000U
#endif
#ifndef SPI_DEMO_ROUNDS
#define SPI_DEMO_ROUNDS 5U
#endif

// Avionics sampling and comms telemetry reporting (milliseconds).
#ifndef TELEMETRY_SAMPLE_INTERVAL_MS
#define TELEMETRY_SAMPLE_INTERVAL_MS 1000U
#endif
#ifndef TELEMETRY_INTERVAL_MS
#define TELEMETRY_INTERVAL_MS 10000U
#endif
#ifndef AVIONICS_STANDALONE_POLL_INTERVAL_MS
#define AVIONICS_STANDALONE_POLL_INTERVAL_MS 10U
#endif

// Image output. Keep JPEG 4:2:2 and the YUV422 converter together.
#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "dev"
#endif
#ifndef CAPTURE_WIDTH
#define CAPTURE_WIDTH 800U
#endif
#ifndef CAPTURE_HEIGHT
#define CAPTURE_HEIGHT 480U
#endif
#ifndef CAPTURE_FPS
#define CAPTURE_FPS 30U
#endif
#ifndef CAPTURE_SETTLE_FRAMES
#define CAPTURE_SETTLE_FRAMES 30U
#endif
#ifndef CAPTURE_TIMEOUT_MS
#define CAPTURE_TIMEOUT_MS 5000U
#endif
// Set to 0 to capture as soon as image ownership permits.
#ifndef PAYLOAD_CAPTURE_INTERVAL_MS
#define PAYLOAD_CAPTURE_INTERVAL_MS 0U
#endif
#define MAX_IMG_FRAME_SIZE (CAPTURE_WIDTH * CAPTURE_HEIGHT * 2U)
#ifndef PAYLOAD_JPEG_QUALITY
#define PAYLOAD_JPEG_QUALITY 75U
#endif
#ifndef PAYLOAD_JPEG_TIMEOUT_MS
#define PAYLOAD_JPEG_TIMEOUT_MS 1000U
#endif
// Set to 1 on both boards to restore whole-image CRC calculation/validation.
#ifndef IMAGE_CRC_ENABLED
#define IMAGE_CRC_ENABLED 0
#endif
// Image mirror/flip: 0 normal, 1 vertical flip, 2 mirror, 3 both.
#ifndef PAYLOAD_CAMERA_MIRROR_FLIP
#define PAYLOAD_CAMERA_MIRROR_FLIP 0U
#endif
// 0/0 selects native 2592x1944
#ifndef PAYLOAD_SENSOR_WIDTH
#define PAYLOAD_SENSOR_WIDTH 0U
#endif
#ifndef PAYLOAD_SENSOR_HEIGHT
#define PAYLOAD_SENSOR_HEIGHT 0U
#endif
#ifndef PAYLOAD_AUTO_EXPOSURE
#define PAYLOAD_AUTO_EXPOSURE 1
#endif
#ifndef PAYLOAD_AUTO_WHITE_BALANCE
#define PAYLOAD_AUTO_WHITE_BALANCE 1
#endif
// Set LED capture time to 0 to disable
#ifndef PAYLOAD_CAPTURE_LED_MS
#define PAYLOAD_CAPTURE_LED_MS 20U
#endif
#ifndef PAYLOAD_UART_BAUD
#define PAYLOAD_UART_BAUD 115200U
#endif
#ifndef IMAGE_TRANSFER_TIMEOUT_MS
#define IMAGE_TRANSFER_TIMEOUT_MS 300U
#endif
#ifndef IMAGE_OFFER_WINDOW_MS
#define IMAGE_OFFER_WINDOW_MS 5U
#endif
#ifndef PAYLOAD_FIRST_IMAGE_NUM
#define PAYLOAD_FIRST_IMAGE_NUM 1U
#endif
#ifndef PAYLOAD_MAX_SAVED_IMAGE_NUM
#define PAYLOAD_MAX_SAVED_IMAGE_NUM 99999U
#endif

#ifndef PAYLOAD_SD_CLOCK_DIV
#define PAYLOAD_SD_CLOCK_DIV 64U
#endif
#ifndef PAYLOAD_SD_BUS_WIDTH
#define PAYLOAD_SD_BUS_WIDTH 1U
#endif
#ifndef PAYLOAD_SD_IO_TIMEOUT_MS
#define PAYLOAD_SD_IO_TIMEOUT_MS 1000U
#endif
#ifndef PAYLOAD_SD_READY_TIMEOUT_MS
#define PAYLOAD_SD_READY_TIMEOUT_MS 3000U
#endif
// Root listing, sample file reads and CSAT.TXT write during startup
#ifndef PAYLOAD_STORAGE_STARTUP_TESTS
#define PAYLOAD_STORAGE_STARTUP_TESTS 1
#endif

#ifndef COMMS_UART_BAUD
#define COMMS_UART_BAUD 921600U
#endif
#ifndef AVIONICS_UART_BAUD
#define AVIONICS_UART_BAUD 115200U
#endif

// Shared SPI mode and framing are fixed by protocol. 
// With existing 64 MHz HSI source: MBR 5 = 1 MHz, 4 = 2 MHz, 3 = 4 MHz, 1 = 16 MHz
#ifndef PAYLOAD_SPI_MASTER_MBR
#define PAYLOAD_SPI_MASTER_MBR 3U
#endif
#ifndef PAYLOAD_SPI_POLL_LIMIT
#define PAYLOAD_SPI_POLL_LIMIT 1000000U
#endif
#ifndef PAYLOAD_SPI_DMA_STOP_LIMIT
#define PAYLOAD_SPI_DMA_STOP_LIMIT 100000U
#endif
#ifndef PAYLOAD_SPI_SLAVE_TIMEOUT_MS
#define PAYLOAD_SPI_SLAVE_TIMEOUT_MS 3000U
#endif
#ifndef PAYLOAD_SPI_CS_SETTLE_CYCLES
#define PAYLOAD_SPI_CS_SETTLE_CYCLES 64U
#endif

#ifndef PAYLOAD_SPI_SCK_SPEED
#define PAYLOAD_SPI_SCK_SPEED 0U
#endif
#ifndef AVIONICS_SPI_SCK_SPEED
#define AVIONICS_SPI_SCK_SPEED 1U
#endif
#ifndef SPI_DEMO_MASTER_MBR
#define SPI_DEMO_MASTER_MBR 7U
#endif

#ifndef COMMS_IMAGE_REQUEST_TIMEOUT_MS
#define COMMS_IMAGE_REQUEST_TIMEOUT_MS 250U
#endif
#ifndef COMMS_TELEMETRY_REQUEST_TIMEOUT_MS
#define COMMS_TELEMETRY_REQUEST_TIMEOUT_MS 500U
#endif

#ifndef COMMS_IMAGE_CRC_BLOCK_SIZE
#define COMMS_IMAGE_CRC_BLOCK_SIZE 1024U
#endif

#ifndef COMMS_PAYLOAD_CS_GPIO_PORT
#define COMMS_PAYLOAD_CS_GPIO_PORT 3U
#endif
#ifndef COMMS_PAYLOAD_CS_GPIO_PIN
#define COMMS_PAYLOAD_CS_GPIO_PIN 14U /* D10, PD14 */
#endif
#ifndef COMMS_PAYLOAD_READY_GPIO_PORT
#define COMMS_PAYLOAD_READY_GPIO_PORT 4U
#endif
#ifndef COMMS_PAYLOAD_READY_GPIO_PIN
#define COMMS_PAYLOAD_READY_GPIO_PIN 9U /* D6, PE9 */
#endif
#ifndef COMMS_AVIONICS_CS_GPIO_PORT
#define COMMS_AVIONICS_CS_GPIO_PORT 5U
#endif
#ifndef COMMS_AVIONICS_CS_GPIO_PIN
#define COMMS_AVIONICS_CS_GPIO_PIN 3U /* D8, PF3 */
#endif
#ifndef COMMS_AVIONICS_READY_GPIO_PORT
#define COMMS_AVIONICS_READY_GPIO_PORT 3U
#endif
#ifndef COMMS_AVIONICS_READY_GPIO_PIN
#define COMMS_AVIONICS_READY_GPIO_PIN 15U /* D9, PD15 */
#endif
#ifndef AVIONICS_SPI_CS_GPIO_PORT
#define AVIONICS_SPI_CS_GPIO_PORT 3U
#endif
#ifndef AVIONICS_SPI_CS_GPIO_PIN
#define AVIONICS_SPI_CS_GPIO_PIN 14U /* D10, PD14 */
#endif
#ifndef AVIONICS_SPI_READY_GPIO_PORT
#define AVIONICS_SPI_READY_GPIO_PORT 3U
#endif
#ifndef AVIONICS_SPI_READY_GPIO_PIN
#define AVIONICS_SPI_READY_GPIO_PIN 15U /* D9, PD15 */
#endif
#ifndef PAYLOAD_SPI_READY_GPIO_PORT
#define PAYLOAD_SPI_READY_GPIO_PORT 3U
#endif
#ifndef PAYLOAD_SPI_READY_GPIO_PIN
#define PAYLOAD_SPI_READY_GPIO_PIN 7U /* D9, PD7 */
#endif

#endif
