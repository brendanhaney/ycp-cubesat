# CubeSat Capstone, Year 1

Fall 2026 - Spring 2027 CubeSat Capstone. This repository includes firmware for
avionics (STM32H753), radio (STM32H753), and camera payload (STM32N6) computers .

# Development Workflow
All sub-systems/features should be developed in separate branches. Please create a branch for each feature. These development branches can be individually merged into the **dev** branch once they have been tested. Only after features are merged into dev and tested together can they be merged into **main**.

## AI Disclosure
ChatGPT was used to create Renode configuration scripts (./venv) and the build system (Makefile, stm32.ld).

References used for register definitions and model behavior:

- [ST STM32H743 device definitions](https://github.com/STMicroelectronics/cmsis-device-h7/blob/master/Include/stm32h743xx.h)
- [ST SPI clock-source definitions](https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Inc/stm32h7xx_ll_rcc.h)
- [Renode STM32H7 SPI model](https://github.com/renode/renode-infrastructure/blob/master/src/Emulator/Peripherals/Peripherals/SPI/STM32H7_SPI.cs)
- [Renode external-control C API](https://renode.readthedocs.io/en/latest/api-description/external-control.html)
