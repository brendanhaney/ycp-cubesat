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

## Avionics I2C sensors and UART

The avionics loop polls the TMP117 thermometer, MS5611 barometer, and SparkFun
SEN-22397 BMI270 IMU on I2C2 once per second. The BMI270 driver uses Bosch's
SensorAPI and startup configuration, with both accelerometer and gyroscope set
to 100 Hz, at ±2 g and ±250 degrees/second. UART reports acceleration in g and
angular velocity in degrees/second along X, Y, and Z. These are sensor readings;
orientation estimation and gyroscope bias calibration are not performed.

The IMU defaults to the unshifted 7-bit address `0x68` in `include/sensors.h`;
initialization also tries `0x69` if the first address does not acknowledge or
contains a different device. Sensor initialization and reads are retried after
errors, and a missing sensor does not prevent polling the others. Sources are
listed at the top of `src/drivers/bmi270.h`; the vendor version and license are
recorded in `src/drivers/bosch/README.md` and `LICENSE`.

Build and flash the connected NUCLEO-H753ZI, then listen through its ST-LINK
virtual COM port at 115200 baud, 8N1:

```sh
make flash-avionics
make uart-listen
```

With multiple serial devices, select the board explicitly:

```sh
make uart-listen UART_ARGS="--port /dev/ttyACM0"
```

Run `make test-imu` for the BMI270 protocol/conversion checks, or `make test` for
the firmware builds, sensor drivers, avionics polling/recovery, and UART checks.

### Rotating cube viewer

`scripts/view_imu.py` reads the same UART as the listener and displays an
orthographic wireframe cube. Dashed world XYZ axes stay fixed; colored body XYZ
arrows rotate with the cube. Both sets originate at the cube's center. The top
right shows the incoming X/Y/Z rates in degrees/second and connection status.

Install the two dependencies and run with automatic ST-LINK port selection:

```sh
python3 -m venv .venv-uart
.venv-uart/bin/python -m pip install -r scripts/requirements-imu.txt
.venv-uart/bin/python scripts/view_imu.py
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
current once-per-second UART output can miss fast motion. Rotation freezes
after three seconds without a gyro sample or immediately on disconnection.
