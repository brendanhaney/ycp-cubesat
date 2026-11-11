ifeq ($(origin CC),default)
CC = arm-none-eabi-gcc
endif
CC ?= arm-none-eabi-gcc
OBJCOPY ?= arm-none-eabi-objcopy
SIZE ?= arm-none-eabi-size

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

BUILD ?= build
PAYLOAD_BOARD ?= NUCLEO-N657X0-Q
PAYLOAD_ORIENTATION ?= FRONT
PAYLOAD_CFLAGS ?= -Os -g3
PAYLOAD_DEFINES ?=

ifneq ($(filter -DCUBESAT_NUCLEO_H753ZI -DCUBESAT_STM32H7,$(CPPFLAGS) $(PAYLOAD_DEFINES)),)
$(error NUCLEO-H753ZI requires STM32H7; payload firmware requires STM32N6)
endif

PAYLOAD_APP = src/stm32/payload
PAYLOAD_DRIVERS = src/drivers/payload
PAYLOAD_VENDOR = src/drivers/stm32n6
PAYLOAD_CMSIS = $(PAYLOAD_VENDOR)/CMSIS
PAYLOAD_DEVICE = $(PAYLOAD_CMSIS)/Device/ST/STM32N6xx
PAYLOAD_HAL = $(PAYLOAD_VENDOR)/HAL
PAYLOAD_BSP = $(PAYLOAD_VENDOR)/BSP
PAYLOAD_CAMERA = src/drivers/camera
PAYLOAD_ISP = $(PAYLOAD_CAMERA)/ISP_Library/isp
PAYLOAD_RTOS = src/drivers/freertos/Source
PAYLOAD_RTOS_PORT = $(PAYLOAD_RTOS)/portable/GCC/ARM_CM55_NTZ/non_secure
PAYLOAD_OBJDIR = $(BUILD)/payload-objects/$(PAYLOAD_BOARD)

# Match Cube's FPv5 configuration, which enables integer MVE instructions only.
PAYLOAD_ARCH = -mcpu=cortex-m55+nomve.fp -mthumb -mcmse -mfpu=fpv5-d16 -mfloat-abi=hard
PAYLOAD_CPPFLAGS = -DSTM32N657xx -DUSE_FULL_ASSERT -DUSE_FULL_LL_DRIVER \
                  -DVECT_TAB_SRAM -DFEAT_FREERTOS \
                  -DSTM32N6570_NUCLEO_REV $(CPPFLAGS) $(PAYLOAD_DEFINES)

ifneq ($(PAYLOAD_BOARD),NUCLEO-N657X0-Q)
$(error Unsupported PAYLOAD_BOARD '$(PAYLOAD_BOARD)'; use NUCLEO-N657X0-Q)
endif
PAYLOAD_LINKER = $(PAYLOAD_APP)/stm32n657.ld
PAYLOAD_BOARD_DIR = $(PAYLOAD_BSP)/STM32N6xx_Nucleo
PAYLOAD_BOARD_SOURCES = $(PAYLOAD_BOARD_DIR)/stm32n6xx_nucleo.c \
                        $(PAYLOAD_BOARD_DIR)/stm32n6xx_nucleo_bus.c

ifeq ($(PAYLOAD_ORIENTATION),FRONT)
PAYLOAD_CPPFLAGS += -DCAMERA_SELFY=1
else ifeq ($(PAYLOAD_ORIENTATION),REAR)
PAYLOAD_CPPFLAGS += -DCAMERA_SELFY=0
else
$(error Unsupported PAYLOAD_ORIENTATION '$(PAYLOAD_ORIENTATION)'; use FRONT or REAR)
endif

PAYLOAD_INCLUDES = $(PAYLOAD_APP) $(PAYLOAD_DRIVERS) \
                   $(PAYLOAD_HAL)/Inc $(PAYLOAD_HAL)/Inc/Legacy \
                   $(PAYLOAD_CMSIS)/Include $(PAYLOAD_DEVICE)/Include \
                   $(PAYLOAD_BOARD_DIR) $(PAYLOAD_RTOS)/include $(PAYLOAD_RTOS_PORT) \
                   $(PAYLOAD_CAMERA) $(PAYLOAD_CAMERA)/sensors \
                   $(PAYLOAD_CAMERA)/sensors/imx335 $(PAYLOAD_ISP)/Inc src/drivers/fatfs

PAYLOAD_SOURCES = $(wildcard $(PAYLOAD_APP)/*.c $(PAYLOAD_DRIVERS)/*.c) \
                  $(wildcard $(PAYLOAD_HAL)/Src/*.c) $(PAYLOAD_BOARD_SOURCES) \
                  $(PAYLOAD_DEVICE)/Source/Templates/system_stm32n6xx_fsbl.c \
                  $(PAYLOAD_CAMERA)/cmw_camera.c $(PAYLOAD_CAMERA)/cmw_utils.c \
                  $(PAYLOAD_CAMERA)/sensors/cmw_imx335.c \
                  $(PAYLOAD_CAMERA)/sensors/imx335/imx335.c \
                  $(PAYLOAD_CAMERA)/sensors/imx335/imx335_reg.c \
                  $(wildcard $(PAYLOAD_ISP)/Src/*.c) \
                  $(PAYLOAD_RTOS)/event_groups.c $(PAYLOAD_RTOS)/list.c \
                  $(PAYLOAD_RTOS)/queue.c $(PAYLOAD_RTOS)/tasks.c $(PAYLOAD_RTOS)/timers.c \
                  $(PAYLOAD_RTOS_PORT)/port.c $(PAYLOAD_RTOS_PORT)/portasm.c \
                  src/drivers/fatfs/ff.c
PAYLOAD_STARTUP = $(PAYLOAD_DEVICE)/Source/Templates/gcc/startup_stm32n657xx.s
PAYLOAD_OBJECTS = $(addprefix $(PAYLOAD_OBJDIR)/,$(PAYLOAD_SOURCES:.c=.o) $(PAYLOAD_STARTUP:.s=.o))
PAYLOAD_FLAGS = $(PAYLOAD_ARCH) $(PAYLOAD_CFLAGS) -std=gnu11 -Wall \
                -fpermitted-flt-eval-methods=c11 \
                -ffunction-sections -fdata-sections -MMD -MP \
                $(PAYLOAD_CPPFLAGS) $(addprefix -I,$(PAYLOAD_INCLUDES))

.PHONY: all FORCE
all: $(BUILD)/payload.elf $(BUILD)/payload.rom

# Record options so changing board, camera orientation, or compile flags rebuilds objects.
$(PAYLOAD_OBJDIR)/flags: FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(CC) $(PAYLOAD_FLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ || mv $@.tmp $@
	@rm -f $@.tmp

$(PAYLOAD_OBJDIR)/%.o: %.c $(PAYLOAD_OBJDIR)/flags scripts/payload.mk
	@mkdir -p $(dir $@)
	$(CC) $(PAYLOAD_FLAGS) -c $< -o $@

$(PAYLOAD_OBJDIR)/%.o: %.s $(PAYLOAD_OBJDIR)/flags scripts/payload.mk
	@mkdir -p $(dir $@)
	$(CC) $(PAYLOAD_ARCH) -x assembler-with-cpp -c $< -o $@

$(BUILD)/payload.elf: $(PAYLOAD_OBJECTS) $(PAYLOAD_LINKER) scripts/payload.mk FORCE
	$(CC) $(PAYLOAD_ARCH) $(PAYLOAD_OBJECTS) -T$(PAYLOAD_LINKER) \
		--specs=nano.specs --specs=nosys.specs -Wl,--gc-sections \
		-Wl,-Map=$(BUILD)/payload.map,--cref -Wl,--print-memory-usage \
		-u _printf_float -lc -lm -lnosys -o $@
	$(SIZE) $@

$(BUILD)/payload.rom: $(BUILD)/payload.elf
	$(OBJCOPY) -O binary $< $@

-include $(PAYLOAD_OBJECTS:.o=.d)
