# ycp-cubesat

Fall 2026 - Spring 2027 CubeSat Capstone. STM32H7 firmware for
avionics, radio, and camera payload computers.

## AI Disclosure
ChatGPT was used to create Renode configuration scripts (./venv) and the build system (Makefile, stm32.ld).

References used for register definitions and model behavior:

- [ST STM32H743 device definitions](https://github.com/STMicroelectronics/cmsis-device-h7/blob/master/Include/stm32h743xx.h)
- [ST SPI clock-source definitions](https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Inc/stm32h7xx_ll_rcc.h)
- [Renode STM32H7 SPI model](https://github.com/renode/renode-infrastructure/blob/master/src/Emulator/Peripherals/Peripherals/SPI/STM32H7_SPI.cs)
- [Renode external-control C API](https://renode.readthedocs.io/en/latest/api-description/external-control.html)
