# ycp-cubesat

Fall 2026 - Spring 2027 CubeSat Capstone. Bare-metal STM32H7 firmware for
avionics, radio, and camera payload computers, with a three-machine Renode demo.

## Build and run

Requires `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, a host C compiler (`cc`),
Python 3, and Renode with its external-control C API (validated with 1.17.0,
build `20260907gitf1dd1b4af`). There are no third-party Python dependencies.
`RENODE_API` defaults to `/opt/renode/tools/external_control_client`; override it
on the make command line for another installation. Run commands from the repo root.

- `make all`: build all three ELF and raw ROM images in `build/`.
- `make sim`: open Renode's graphical monitor and all three USART1 analyzer
  windows. The SPI demo runs once and pauses; the windows and C peripheral
  adapter stay open for inspection until you quit Renode. Use `start` in the
  monitor to resume (the demo firmware idles after its five rounds). Logs are
  saved to `build/spi-all.log` and `build/spi-all-adapter.log`.
- `python3 venv/run_spi.py --headless`: run the built demo without windows,
  print the captured output, and exit.
- `make test`: build and run the standalone C model tests and headless Renode
  integration checks; full output is saved to `build/spi-integration.log`.
- `make sim-avionics`, `make sim-radio`, `make sim-payload`: run one board for
  initialization checks with the GUI. These advance one millisecond and pause;
  the board waits for its missing peers. Use `make sim` for communication.

## SPI connection

All computers use SPI4: mode 0 (CPOL=0, CPHA=0), 8-bit frames, MSB first,
full duplex, FIFO threshold of one byte. Avionics is the only master. The SPI4
kernel clock is selected from HSI, with a /256 SPI divider (250 kHz with the
reset 64 MHz HSI clock and HSI divider of one).

Proposed STM32H743 pin assignment (confirm against the actual board schematic):

| Signal | Avionics | Radio | Payload |
| --- | --- | --- | --- |
| SCK | PE2, AF5 | PE2, AF5 | PE2, AF5 |
| MOSI | PE6, AF5 | PE6, AF5 | PE6, AF5 |
| MISO | PE5, AF5 | PE5, AF5 | PE5, AF5 |
| Radio select | PE3, GPIO output | PE4, AF5 NSS | — |
| Payload select | PE4, GPIO output | — | PE4, AF5 NSS |
| Radio ready | PB0, GPIO input | PB0, GPIO output | — |
| Payload ready | PB1, GPIO input | — | PB0, GPIO output |

Connect grounds as well. Chip selects are active low and idle high. Each slave
uses hardware NSS, so only the selected slave drives the shared MISO line.
Slave NSS inputs have pull-ups and master ready inputs have pull-downs.
External CS pull-ups are recommended to maintain deselection during board reset.
The ready lines are required by this polling implementation, not data transports.

`src/drivers/spi.c` follows the existing register-level UART driver organization.
`spi4_init()` configures clocks, pins and role. `spi4_master_transfer()` and
`spi4_slave_transfer()` exchange arbitrary byte buffers, returning 0 on success
or -1 on invalid arguments, timeout or a peripheral error. Buffer lengths must
match at both ends; transmit and receive pointers must be non-null.

Transactions contain 1–8 bytes, matching SPI4's eight-byte FIFO. Each endpoint
preloads the entire outgoing frame. A slave raises ready only after preloading;
the master waits for ready, asserts that slave's CS, and starts SPI. Every clocked
byte transfers MOSI and MISO simultaneously. Both ends drain RX and check EOT;
the slave lowers ready, the master observes it and raises CS, then the slave can
rearm. CS therefore brackets exactly one transfer, and ready cannot be mistaken
for readiness for the next transfer. Replies must be prepared before the transfer;
a response computed from a request belongs in a subsequent transaction.

The slave can wait indefinitely while idle. Once selected, polling has a bounded
iteration budget (`SPI_POLL_LIMIT`), as do master waits. This is not a calibrated
wall-clock timeout. Errors deselect the master CS and disable the local SPI;
the demonstration stops on failure instead of attempting protocol recovery.
Larger structured payloads will need agreed framing across multiple transactions
or a future interrupt/DMA implementation; increasing the FIFO limit alone is
incorrect. There is no string-specific behavior in the driver.

## Demonstration and validation

Avionics alternates between radio and payload for five rounds. Each frame is
eight bytes, including a terminating NUL:

| Selected slave | Avionics sends | Slave simultaneously sends |
| --- | --- | --- |
| Radio | `A>RADIO` | `RADIO>A` |
| Payload | `A>PAYLD` | `PAYLD>A` |

Each CPU checks all eight received bytes before printing a success line on its
local USART1. USART1 is only an observation console; no UART connects the boards.
After five rounds, each CPU prints `SPI demo complete` and idles.

The automated suite first runs C model tests, then asserts all four received
strings occur five times, all three CPUs complete, and the SPI bus clocks exactly 80 bytes: 40 bytes and five
NSS-delimited transactions per slave, with no selection, underrun or overrun
errors. It then pauses the CPUs and exercises the real master model's MMIO and
CS GPIOs using independent one- and eight-byte binary frames (including zero,
0x80 and 0xFF). It checks both directions, unchanged counters on the unselected
slave, and rejection of no-CS and simultaneous-CS transfers. Slave-model checks
also cover underrun, truncated transactions and FIFO capacity. No automated tests
existed before this SPI suite.

## Renode representation and limits

Renode's stock `SPI.STM32H7_SPI` explicitly supports only master mode; it cannot
be used as the other CPU's SPI slave. The avionics board keeps this stock model.
The custom models are implemented in C:

- `venv/peripherals/spi_model.c` implements the slave registers, bounded FIFOs,
  transfer counters, and chip-select-controlled SPI bus.
- `venv/peripherals/spi_renode.c` connects that model to Renode's installed C API.
- `tests/spi_assertions.c` contains the integration assertions; standalone model
  tests live in `tests/test_spi_model.c`.

The radio and payload `.resc` scripts place Renode's built-in
`Bus.ExternalControlBusPeripheral` at the real SPI4 MMIO address. Avionics uses
its stock SPI4 master with an `SPI.ExternalControlSPIPeripheral` attached.
`venv/run_spi.py` launches Renode and the compiled C adapter, captures logs, and
cleans up both processes. `connect.resc` starts the external-control server. The
adapter registers all SPI, MMIO and GPIO callbacks before advancing the CPUs.
Start the simulation with the make targets so that the C adapter is present;
loading the `.resc` files alone does not provide a running slave model.

The C adapter uses Renode's local external-control socket for co-simulation.
This socket carries simulator peripheral callbacks (MMIO accesses, GPIO changes,
and SPI byte transfers); it is not exposed to firmware as a data transport.
The communication path remains:

```
avionics TXDR -> STM32H7_SPI -> external SPI callback -> C bus -> slave RX FIFO -> RXDR
avionics RXDR <- STM32H7_SPI <- external SPI callback <- C bus <- slave TX FIFO <- TXDR
```

The model never reads CPU RAM, generates application replies, or knows the demo
strings. USART1 remains a local logging console only. CS GPIO callbacks select
the slave and define transaction boundaries; ready GPIO callbacks connect each
slave's PB0 to the corresponding avionics input. The same firmware images run on
all three emulated CPUs without simulation-specific conditionals.

The C slave implements SPE, TSIZE, 8-bit TX/RX FIFOs, RXP/TXP/EOT, and diagnostic
error cases used here. It is a limited peripheral model, not a complete STM32
SPI implementation; interrupts, DMA, CRC, other frame formats, and all
reset/clock-gating behavior are outside its scope. Firmware transaction limits
and physical pin assignments are unchanged by the C conversion.

This is byte-level functional SPI emulation, not electrical or cycle-accurate
SCK/MOSI/MISO simulation. Peripheral callbacks are synchronous;
clock edges, setup/hold timing, MISO tri-state electrical behavior, and the real
250 kHz transfer duration are not simulated. The stock model tags several clock,
NSS, and status-clear fields without implementing them. Renode consequently logs
warnings for SPI45SEL, MBR, AFCNTR, SSM, SSI and some IFCR bits; these are retained
in test logs. Correct hardware configuration is still written by firmware.
Physical-board timing, pin availability and signal integrity remain unverified.
The existing minimal startup/vector table and UART initialization are unchanged;
a complete physical-board bring-up remains separate work.

References used for register definitions and model behavior:

- [ST STM32H743 device definitions](https://github.com/STMicroelectronics/cmsis-device-h7/blob/master/Include/stm32h743xx.h)
- [ST SPI clock-source definitions](https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Inc/stm32h7xx_ll_rcc.h)
- [Renode STM32H7 SPI model](https://github.com/renode/renode-infrastructure/blob/master/src/Emulator/Peripherals/Peripherals/SPI/STM32H7_SPI.cs)
- [Renode external-control C API](https://renode.readthedocs.io/en/latest/api-description/external-control.html)
