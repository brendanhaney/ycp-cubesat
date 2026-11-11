CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

.DEFAULT_GOAL := all

CFLAGS = -mcpu=cortex-m7 -mthumb -O2 -Wall -Iinclude -Isrc
#LDFLAGS = -Tstm32.ld -Wl,--gc-sections
LDFLAGS = -Tstm32.ld -Wl,--gc-sections --specs=nosys.specs -Wl,--defsym=end=_end

COMMON = $(wildcard src/drivers/*.c) \
         $(wildcard src/utils/*.c)

BUILD = ./build

RESC_DIR = ./venv
NUCLEO_BUILD ?= $(BUILD)/nucleo-h753zi
PAYLOAD_BOARD ?= NUCLEO-N657X0-Q

define build
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON) $(wildcard src/stm32/$(1)/*.c) \
		$(LDFLAGS) -o $(BUILD)/$(1).elf
	$(OBJCOPY) -O binary $(BUILD)/$(1).elf $(BUILD)/$(1).rom
endef

.PHONY: all avionics radio payload clean sim sim-headless sim-avionics sim-radio sim-payload test

avionics:
	$(call build,avionics)

radio:
	$(call build,radio)

payload:
	$(MAKE) -f scripts/payload.mk BUILD="$(BUILD)" PAYLOAD_BOARD="$(PAYLOAD_BOARD)"

all: avionics radio payload

clean:
	rm -rf $(BUILD)

HOST_CC ?= cc
RENODE_API ?= /opt/renode/tools/external_control_client
HOST_CFLAGS = -std=c11 -D_DEFAULT_SOURCE -O2 -Wall -Wextra -Werror -fno-strict-aliasing -pthread
SPI_MODEL = $(RESC_DIR)/peripherals/spi_model.c
SPI_ADAPTER = $(RESC_DIR)/peripherals/spi_renode.c
SPI_HEADERS = $(RESC_DIR)/peripherals/spi_model.h $(RESC_DIR)/peripherals/spi_renode.h
API_SOURCES = $(RENODE_API)/lib/renode_api.c $(RENODE_API)/lib/connection.c
API_HEADERS = $(wildcard $(RENODE_API)/include/*.h $(RENODE_API)/lib/*.h)
HOST_INCLUDES = -I$(RESC_DIR)/peripherals -I$(RENODE_API)/include

$(BUILD)/spi-renode: $(SPI_MODEL) $(SPI_ADAPTER) $(SPI_HEADERS) $(API_SOURCES) $(API_HEADERS)
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_INCLUDES) $(SPI_MODEL) $(SPI_ADAPTER) $(API_SOURCES) -o $@

$(BUILD)/spi-integration: $(SPI_MODEL) $(SPI_ADAPTER) $(SPI_HEADERS) $(API_SOURCES) $(API_HEADERS) tests/spi_assertions.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_INCLUDES) -DSPI_INTEGRATION_TEST $(SPI_MODEL) $(SPI_ADAPTER) $(API_SOURCES) tests/spi_assertions.c -o $@

$(BUILD)/test-spi-model: $(SPI_MODEL) $(RESC_DIR)/peripherals/spi_model.h tests/test_spi_model.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -I$(RESC_DIR)/peripherals $(SPI_MODEL) tests/test_spi_model.c -o $@

sim: all $(BUILD)/spi-renode sim-payload
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD) --module avionics-radio

sim-headless: all $(BUILD)/spi-renode sim-payload
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD) --module avionics-radio --headless

sim-avionics: avionics $(BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD) --module avionics

sim-radio: radio $(BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD) --module radio

sim-payload: payload
	python3 -B tests/test_payload_renode.py --build $(BUILD)/payload-renode

# The legacy firmware SPI integration test expects an avionics SPI demo;
# current avionics firmware runs the sensor polling loop instead.
test: all test-uart test-i2c test-thermometer test-barometer test-payload $(BUILD)/test-spi-model
	$(BUILD)/test-spi-model

FLASH_TOOL = st-flash
FLASH_ADDR = 0x08000000

.PHONY: nucleo-avionics nucleo-radio nucleo-payload flash-avionics flash-radio flash-payload uart-listen test-uart test-i2c test-thermometer test-barometer

# Keep board binaries separate from the USART1 images used by Renode.
nucleo-avionics nucleo-radio: nucleo-%:
	$(MAKE) $* BUILD="$(NUCLEO_BUILD)" CPPFLAGS="$(CPPFLAGS) -DCUBESAT_NUCLEO_H753ZI"

nucleo-payload: payload

flash-avionics flash-radio: flash-%: nucleo-%
	$(FLASH_TOOL) --reset write $(NUCLEO_BUILD)/$*.rom $(FLASH_ADDR)

flash-payload:
	@echo "Payload flashing implemented in another branch"

uart-listen:
	python3 scripts/listen_uart.py $(UART_ARGS)

test-uart:
	python3 -B tests/test_uart.py
	python3 -B tests/test_listen_uart.py

test-i2c:
	python3 -B tests/test_i2c.py

test-thermometer:
	python3 -B tests/test_tmp117.py
	python3 -B tests/test_avionics.py

test-barometer:
	python3 -B tests/test_ms5611.py

.PHONY: test-payload test-payload-drivers

test-payload: payload test-payload-drivers
	python3 -B tests/test_payload_layout.py --elf $(BUILD)/payload.elf --board $(PAYLOAD_BOARD)
	python3 -B tests/test_payload_renode.py --build $(BUILD)/payload-renode

test-payload-drivers:
	python3 -B tests/test_camera_conversion.py
	python3 -B tests/test_payload_storage.py
