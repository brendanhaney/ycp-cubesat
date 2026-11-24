#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <config.h>
#include <stddef.h>

#if COMMS_UART_BAUD < 977U || COMMS_UART_BAUD > 4000000U || \
    AVIONICS_UART_BAUD < 977U || AVIONICS_UART_BAUD > 4000000U
#error "UART baud is outside the supported range"
#endif

void uart_init(void);
/* Change the idle H753 console baud; call immediately after uart_init(). */
int uart_set_baud(unsigned int baud);
void uart_write(const void *data, size_t length);

void uart_write_str(const char *str);

/* Formats: %d, %f, %x. */
void uart_printf(const char *fmt, ...);

#endif
