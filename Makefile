CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS = -mcpu=cortex-m7 -mthumb -O2 -Wall -Iinclude -Isrc
LDFLAGS = -Tstm32.ld -Wl,--gc-sections

COMMON = $(wildcard src/drivers/*.c) \
         $(wildcard src/utils/*.c)

BUILD = ./build

RESC_DIR = ./venv

define build
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(COMMON) $(wildcard src/stm32/$(1)/*.c) \
		$(LDFLAGS) -o $(BUILD)/$(1).elf
	$(OBJCOPY) -O binary $(BUILD)/$(1).elf $(BUILD)/$(1).rom
endef

.PHONY: all avionics radio payload clean sim sim-avionics sim-radio sim-payload test

avionics:
	$(call build,avionics)

radio:
	$(call build,radio)

payload:
	$(call build,payload)

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

sim: all $(BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD)

sim-avionics: avionics $(BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD) --module avionics

sim-radio: radio $(BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD) --module radio

sim-payload: payload $(BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(BUILD) --module payload

test: all $(BUILD)/spi-integration $(BUILD)/test-spi-model
	$(BUILD)/test-spi-model
	python3 tests/test_spi.py --build $(BUILD)
