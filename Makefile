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

.PHONY: all avionics radio payload clean sim sim-avionics sim-radio sim-payload

avionics:
	$(call build,avionics)

radio:
	$(call build,radio)

payload:
	$(call build,payload)

all: avionics radio payload

clean:
	rm -rf $(BUILD)

sim: all
	renode $(RESC_DIR)/common.resc \
	       -e 'i @$(RESC_DIR)/avionics.resc; i @$(RESC_DIR)/radio.resc; i @$(RESC_DIR)/payload.resc; i @$(RESC_DIR)/start.resc'

sim-avionics: avionics
	renode $(RESC_DIR)/common.resc \
	       -e 'i @$(RESC_DIR)/avionics.resc; i @$(RESC_DIR)/start.resc'

sim-radio: radio
	renode $(RESC_DIR)/common.resc \
	       -e 'i @$(RESC_DIR)/radio.resc; i @$(RESC_DIR)/start.resc'

sim-payload: payload
	renode $(RESC_DIR)/common.resc \
	       -e 'i @$(RESC_DIR)/payload.resc; i @$(RESC_DIR)/start.resc'