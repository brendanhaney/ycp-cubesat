CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS = -mcpu=cortex-m7 -mthumb -O2 -Wall -Iinclude -Isrc
LDFLAGS = -Tstm32.ld -Wl,--gc-sections

COMMON = $(wildcard src/drivers/*.c) \
         $(wildcard src/utils/*.c)

BUILD = ./build

define build
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(COMMON) $(wildcard src/stm32/$(1)/*.c) \
		$(LDFLAGS) -o $(BUILD)/$(1).elf
	$(OBJCOPY) -O binary $(BUILD)/$(1).elf $(BUILD)/$(1).rom
endef

avionics:
	$(call build,avionics)

radio:
	$(call build,radio)

payload:
	$(call build,payload)

all: avionics radio payload

clean:
	rm -rf $(BUILD)
