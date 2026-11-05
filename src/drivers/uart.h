#ifndef UART_DRIVER_H
#define UART_DRIVER_H

void uart_init(void);

void uart_write_str(const char *str);

/* Formats: %d, %f, %x. */
void uart_printf(const char *fmt, ...);

#endif
