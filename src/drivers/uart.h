#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stddef.h>

void uart_init(void);
/* Change the idle H753 console baud; call immediately after uart_init(). */
int uart_set_baud(unsigned int baud);
void uart_write(const void *data, size_t length);

void uart_write_str(const char *str);

/* Formats: %d, %f, %x. */
void uart_printf(const char *fmt, ...);

#endif
