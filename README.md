# CubeSat Capstone, Year 1

Fall 2026 - Spring 2027 CubeSat Capstone. This repository includes firmware for
avionics (STM32H753), radio (STM32H753), and camera payload (STM32N6) computers .

## Linux package requirements

| Use | Arch Linux | Debian Linux |
| --- | --- | --- |
| Firmware and SPI adapter builds | `make`, `gcc`, `arm-none-eabi-gcc`, `arm-none-eabi-binutils`, `arm-none-eabi-newlib` | `build-essential`, `gcc-arm-none-eabi`, `binutils-arm-none-eabi`, `libnewlib-arm-none-eabi` |
| Renode simulation | `python`, `openssl-3`, [Renode Linux portable][renode-release] | `python3`, `libssl3t64`, [Renode Linux portable][renode-release] |
| Renode desktop UI | `gtk3` | `libgtk-3-0t64` |
| USB tools and programming | `usbutils`, `stlink`, [STM32CubeProgrammer][cube-programmer] (payload) | `usbutils`, `stlink-tools`, [STM32CubeProgrammer][cube-programmer] (payload) |
| UART tools and viewers | `python-pyserial`, `python-pygame`, `python-pillow`, `tk` | `python3-serial`, `python3-pygame`, `python3-pil`, `python3-pil.imagetk`, `python3-tk` |
| Python virtual environment | `python` | `python3-venv` |

[renode-release]: https://github.com/renode/renode/releases/tag/v1.17.1
[cube-programmer]: https://www.st.com/en/development-tools/stm32cubeprog.html

# Development Workflow
All sub-systems/features should be developed in separate branches. Please create a branch for each feature. These development branches can be individually merged into the **dev** branch once they have been tested. Only after features are merged into dev and tested together can they be merged into **main**.

## AI Disclosure
ChatGPT was used to create Renode configuration scripts (./venv) and the build system (Makefile, stm32.ld).

References used for register definitions and model behavior:

- [ST STM32H743 device definitions](https://github.com/STMicroelectronics/cmsis-device-h7/blob/master/Include/stm32h743xx.h)
- [ST SPI clock-source definitions](https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Inc/stm32h7xx_ll_rcc.h)
- [Renode STM32H7 SPI model](https://github.com/renode/renode-infrastructure/blob/master/src/Emulator/Peripherals/Peripherals/SPI/STM32H7_SPI.cs)
- [Renode external-control C API](https://renode.readthedocs.io/en/latest/api-description/external-control.html)

### Avionics telemetry viewer

`scripts/view_avionics.py` reads the same UART as the listener and displays an
orthographic wireframe cube. Dashed world XYZ axes stay fixed; colored body XYZ
arrows rotate with the cube. Both sets originate at the cube's center. The top
right shows the incoming gyroscope X/Y/Z rates in degrees/second, accelerometer
X/Y/Z readings in g, both thermometer readings in Celsius, and barometer pressure in hPa.
Each sensor has its own sample-age status; missing readings show a placeholder
and stale readings are marked. Only gyroscope rates affect the cube's rotation.

Estimated altitude in meters appears below pressure and shares its sample status.
It uses the standard-atmosphere barometric formula with a default sea-level
reference of 1013.25 hPa (pressure altitude). Use `--sea-level-pressure 1018.0` to
set your local sea-level reference in hPa; weather and atmospheric conditions
affect the estimate. Missing or nonpositive pressure shows `-- m`.

Install the two dependencies and run with automatic ST-LINK port selection:

```sh
python3 -m venv .venv-uart
.venv-uart/bin/python -m pip install -r scripts/requirements-imu.txt
.venv-uart/bin/python scripts/view_avionics.py
```

Use `--port /dev/ttyACM0` (or `--port COM3` on Windows) to select a port,
`--list-ports` to list devices, or `--demo` to preview without hardware. If the
dependencies are already installed, `make uart-view` also launches the viewer
and accepts the same `UART_ARGS` as `make uart-listen`. Close the UART listener
before starting the viewer so the two programs do not compete for sensor data.

Startup is zero rotation, assuming the gyroscope bias is already zeroed.
**R** resets the cube's orientation, **Space** pauses/resumes rotation while
still showing incoming readings, and **Esc** closes the window. The world frame
is the startup/reset reference. Gyroscope integration accumulates drift; the
UART sampling can miss fast motion. Rotation freezes
after three seconds without a gyro sample or immediately on disconnection.
