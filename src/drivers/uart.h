// SOURCES
// https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
// https://www.st.com/resource/en/user_manual/um2407-stm32h7-nucleo144-boards-mb1364-stmicroelectronics.pdf
// https://www.st.com/resource/en/datasheet/stm32h753vi.pdf

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

void uart_init(void);

void uart_write_str(const char *str);

// Functions exactly like standard C printf
// Implemented format specifiers: 
//      %d (int), %f (float), %x (hex)
void uart_printf(const char *fmt, ...);

#endif
