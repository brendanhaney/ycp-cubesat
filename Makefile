ifeq ($(origin CC),default)
CC = arm-none-eabi-gcc
endif
CC ?= arm-none-eabi-gcc
OBJCOPY ?= arm-none-eabi-objcopy

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CFLAGS ?= -O2 -Wall
LDFLAGS ?= -Wl,--gc-sections --specs=nosys.specs -Wl,--defsym=end=_end
BUILD ?= ./build
H7_BUILD ?= $(BUILD)/stm32h7
N6_BUILD ?= $(BUILD)/stm32n6
RENODE_BUILD ?= $(BUILD)/renode
NUCLEO_BUILD ?= $(BUILD)/nucleo-h753zi
PAYLOAD_BOARD ?= NUCLEO-N657X0-Q
RESC_DIR = ./venv

FIRMWARE_CFLAGS = $(filter-out -mcpu=% -march=% -mfpu=% -mfloat-abi=% -mcmse -mno-cmse -mthumb -marm,$(CFLAGS))
stm32h7_FLAGS = -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -DCUBESAT_STM32H7
stm32n6_FLAGS = -mcpu=cortex-m55+nomve -mthumb -mfloat-abi=soft -mcmse -DCUBESAT_STM32N6
COMMON_SOURCES = src/drivers/uart.c src/drivers/spi.c \
                 $(wildcard src/utils/*.c) src/drivers/startup.c
stm32h7_SOURCES = src/drivers/i2c.c src/drivers/tmp117.c src/drivers/ms5611.c \
                  src/drivers/bmi270.c src/drivers/bosch/bmi2.c src/drivers/bosch/bmi270.c \
                  $(wildcard src/core/*.c)
HEADERS = $(wildcard include/*.h src/drivers/*.h src/drivers/bosch/*.h src/utils/*.h src/core/*.h)

define firmware
$(3)/$(1).ld: linker/linker.ld.S include/config.h Makefile FORCE
	@mkdir -p $$(@D)
	$(CC) $(CPPFLAGS) -Iinclude $(FIRMWARE_CFLAGS) $$($(2)_FLAGS) $(4) \
		-E -P -x c $$< -o $$@

$(3)/$(1).elf: $(COMMON_SOURCES) $$($(2)_SOURCES) $(wildcard src/stm32/$(1)/*.c) linker/startup.S $(3)/$(1).ld $(HEADERS) Makefile FORCE
	@mkdir -p $$(@D)
	$(CC) $(CPPFLAGS) -Iinclude -Isrc $(FIRMWARE_CFLAGS) $$($(2)_FLAGS) $(4) \
		-ffunction-sections -fdata-sections -nostartfiles $(COMMON_SOURCES) $$($(2)_SOURCES) \
		$(wildcard src/stm32/$(1)/*.c) linker/startup.S \
		$(LDFLAGS) -T$(3)/$(1).ld -Wl,-Map,$(3)/$(1).map $(LDLIBS) -o $$@

$(3)/$(1).rom: $(3)/$(1).elf
	$(OBJCOPY) -O binary $$< $$@
endef

$(eval $(call firmware,avionics,stm32h7,$(H7_BUILD)))
$(eval $(call firmware,radio,stm32h7,$(H7_BUILD)))
$(eval $(call firmware,payload,stm32n6,$(N6_BUILD)))
$(foreach module,avionics radio payload,$(eval $(call firmware,$(module),stm32h7,$(RENODE_BUILD))))
$(foreach module,avionics radio,$(eval $(call firmware,$(module),stm32h7,$(NUCLEO_BUILD),-DCUBESAT_NUCLEO_H753ZI)))

payload:
	$(MAKE) -f scripts/payload.mk BUILD="$(BUILD)" PAYLOAD_BOARD="$(PAYLOAD_BOARD)"
.PHONY: all avionics radio payload renode renode-avionics renode-radio renode-payload clean FORCE
.PHONY: nucleo-avionics nucleo-radio nucleo-payload

all: avionics radio payload
avionics: $(H7_BUILD)/avionics.rom
radio: $(H7_BUILD)/radio.rom
payload: $(N6_BUILD)/payload.rom
renode: renode-avionics renode-radio renode-payload
renode-avionics: $(RENODE_BUILD)/avionics.rom
renode-radio: $(RENODE_BUILD)/radio.rom
renode-payload: $(RENODE_BUILD)/payload.rom
nucleo-avionics: $(NUCLEO_BUILD)/avionics.rom
nucleo-radio: $(NUCLEO_BUILD)/radio.rom
nucleo-payload: payload
FORCE:

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

$(RENODE_BUILD)/spi-renode: $(SPI_MODEL) $(SPI_ADAPTER) $(SPI_HEADERS) $(API_SOURCES) $(API_HEADERS)
	@mkdir -p $(RENODE_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_INCLUDES) $(SPI_MODEL) $(SPI_ADAPTER) $(API_SOURCES) -o $@

$(RENODE_BUILD)/spi-integration: $(SPI_MODEL) $(SPI_ADAPTER) $(SPI_HEADERS) $(API_SOURCES) $(API_HEADERS) tests/spi_assertions.c
	@mkdir -p $(RENODE_BUILD)
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

sim: renode $(RENODE_BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(RENODE_BUILD)

sim-payload: payload
	python3 -B tests/test_payload_renode.py --build $(BUILD)/payload-renode

# The legacy firmware SPI integration test expects an avionics SPI demo;
# current avionics firmware runs the sensor polling loop instead.
test: all test-uart test-i2c test-thermometer test-barometer test-payload $(BUILD)/test-spi-model
	$(BUILD)/test-spi-model
sim-headless: renode $(RENODE_BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(RENODE_BUILD) --headless

sim-avionics: renode-avionics $(RENODE_BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(RENODE_BUILD) --module avionics

sim-radio: renode-radio $(RENODE_BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(RENODE_BUILD) --module radio

sim-payload: renode-payload $(RENODE_BUILD)/spi-renode
	python3 $(RESC_DIR)/run_spi.py --build $(RENODE_BUILD) --module payload

# Keep board binaries separate from the USART1 images used by Renode.
nucleo-avionics nucleo-radio: nucleo-%:
	$(MAKE) $* BUILD="$(NUCLEO_BUILD)" CPPFLAGS="$(CPPFLAGS) -DCUBESAT_NUCLEO_H753ZI"

nucleo-payload: payload

flash-avionics flash-radio: flash-%: nucleo-%
	$(FLASH_TOOL) --reset write $(NUCLEO_BUILD)/$*.rom $(FLASH_ADDR)
test: all test-build test-platform test-uart test-i2c test-thermometer test-barometer test-imu $(BUILD)/test-spi-model
	$(BUILD)/test-spi-model

test-build:
	CC="$(CC)" OBJCOPY="$(OBJCOPY)" python3 -B tests/test_build.py

test-platform:
	python3 -B tests/test_stm32n6.py

H7_FLASH_TOOL ?= st-flash
H7_FLASH_ADDR ?= 0x08000000
STM32_PROGRAMMER_BIN ?= $(HOME)/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin
STM32_PROGRAMMER ?= $(or $(shell command -v STM32_Programmer_CLI 2>/dev/null),$(STM32_PROGRAMMER_BIN)/STM32_Programmer_CLI)
STM32_PROGRAMMER_DIR ?= $(shell dirname "$$(command -v "$(STM32_PROGRAMMER)" 2>/dev/null)")/
STM32_SIGNING_TOOL ?= $(or $(shell command -v STM32_SigningTool_CLI 2>/dev/null),$(STM32_PROGRAMMER_DIR)STM32_SigningTool_CLI)
N6_EXTERNAL_LOADER ?= $(STM32_PROGRAMMER_DIR)ExternalLoader/MX25UM51245G_STM32N6570-NUCLEO.stldr
N6_FLASH_ADDR ?= 0x70000000
N6_HEADER_VERSION ?= 2.3
N6_SIGN_ALIGN ?= -align
N6_PROGRAMMER_ARGS ?= -c port=SWD mode=HOTPLUG ap=1

.PHONY: signed-payload flash-avionics flash-radio flash-payload flash-payload-external run-payload n6-help uart-listen uart-view
.PHONY: test-uart test-i2c test-thermometer test-barometer test-imu

$(N6_BUILD)/payload.bin: $(N6_BUILD)/payload.elf
	$(OBJCOPY) -O binary $< $@

signed-payload: payload $(N6_BUILD)/payload.bin
	@command -v "$(STM32_SIGNING_TOOL)" >/dev/null || { echo "Install STM32CubeProgrammer and set STM32_SIGNING_TOOL to its signing executable." >&2; exit 1; }
	"$(STM32_SIGNING_TOOL)" -s -bin "$(N6_BUILD)/payload.bin" -nk -of 0x80000000 -t fsbl \
		-hv $(N6_HEADER_VERSION) $(N6_SIGN_ALIGN) -o "$(N6_BUILD)/payload-trusted.bin"

flash-avionics: nucleo-avionics
	$(H7_FLASH_TOOL) --reset write $(NUCLEO_BUILD)/avionics.rom $(H7_FLASH_ADDR)

flash-radio: nucleo-radio
	$(H7_FLASH_TOOL) --reset write $(NUCLEO_BUILD)/radio.rom $(H7_FLASH_ADDR)

flash-payload: run-payload

run-payload: payload
	@command -v "$(STM32_PROGRAMMER)" >/dev/null || { echo "Install STM32CubeProgrammer and set STM32_PROGRAMMER to its CLI executable." >&2; exit 1; }
	python3 -B scripts/run_payload.py --programmer "$(STM32_PROGRAMMER)" --elf "$(N6_BUILD)/payload.elf" -- $(N6_PROGRAMMER_ARGS)

flash-payload-external: signed-payload
	@command -v "$(STM32_PROGRAMMER)" >/dev/null || { echo "Install STM32CubeProgrammer and set STM32_PROGRAMMER to its CLI executable." >&2; exit 1; }
	@test -f "$(N6_EXTERNAL_LOADER)" || { echo "Set N6_EXTERNAL_LOADER to MX25UM51245G_STM32N6570-NUCLEO.stldr from STM32CubeProgrammer." >&2; exit 1; }
	"$(STM32_PROGRAMMER)" $(N6_PROGRAMMER_ARGS) -el "$(N6_EXTERNAL_LOADER)" \
		-w "$(N6_BUILD)/payload-trusted.bin" $(N6_FLASH_ADDR) -v

n6-help:
	@echo "make payload: build the SRAM executable at $(N6_BUILD)/payload.elf."
	@echo "make run-payload (or make flash-payload): load, verify and start the payload in SRAM over ST-LINK."
	@echo "Repeat run-payload after reset or power loss. No signing tool or external loader is needed for SRAM."
	@echo "make signed-payload: create $(N6_BUILD)/payload-trusted.bin with STM32CubeProgrammer's signing tool."
	@echo "make flash-payload-external: program persistent external flash; standalone boot requires BOOT0=0 and BOOT1=0."
	@echo "N6_SIGN_ALIGN=-align is required for CubeProgrammer 2.21+; use N6_SIGN_ALIGN= with older versions."
	@echo "N6_HEADER_VERSION defaults to 2.3; select the header version required by your silicon revision."

flash-payload:
	@echo "Payload flashing implemented in another branch"

uart-listen:
	python3 scripts/listen_uart.py $(UART_ARGS)

uart-view:
	python3 scripts/view_avionics.py $(UART_ARGS)

test-uart:
	python3 -B tests/test_uart.py
	python3 -B tests/test_listen_uart.py
	python3 -B tests/test_view_avionics.py

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
test-imu:
	python3 -B tests/test_bmi270.py
