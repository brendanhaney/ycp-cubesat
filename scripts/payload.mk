CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

.DEFAULT_GOAL := all

BUILD ?= build
PAYLOAD_BOARD ?= NUCLEO-N657X0-Q
PAYLOAD_ORIENTATION ?= FRONT
PAYLOAD_CFLAGS ?= -Os -g3
PAYLOAD_DEFINES ?=

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
PAYLOAD_UVCL = src/drivers/uvcl
PAYLOAD_USBX = src/drivers/usbx
PAYLOAD_OBJDIR = $(BUILD)/payload-objects/$(PAYLOAD_BOARD)

# Match Cube's FPv5 configuration, which enables integer MVE instructions only.
PAYLOAD_ARCH = -mcpu=cortex-m55+nomve.fp -mthumb -mcmse -mfpu=fpv5-d16 -mfloat-abi=hard
PAYLOAD_CPPFLAGS = -DSTM32N657xx -DUSE_FULL_ASSERT -DUSE_FULL_LL_DRIVER \
                  -DVECT_TAB_SRAM -DFEAT_FREERTOS -DTX_MAX_PARALLEL_NETWORKS=1 \
                  -DUSE_IMX335_SENSOR -DUSE_VD66GY_SENSOR \
                  -DUSE_VD55G1_SENSOR -DUSE_VD1943_SENSOR \
                  -DUVC_LIB_USE_DMA -DUVC_LIB_USE_USBX -DUX_INCLUDE_USER_DEFINE_FILE \
                  -DUSBL_PACKET_PER_MICRO_FRAME=3 -DUX_STANDALONE \
                  -DUVCL_USBX_USE_FREERTOS $(PAYLOAD_DEFINES)

ifeq ($(PAYLOAD_BOARD),NUCLEO-N657X0-Q)
PAYLOAD_CPPFLAGS += -DSTM32N6570_NUCLEO_REV
PAYLOAD_LINKER = $(PAYLOAD_APP)/stm32n657.ld
PAYLOAD_BOARD_DIR = $(PAYLOAD_BSP)/STM32N6xx_Nucleo
PAYLOAD_BOARD_SOURCES = $(PAYLOAD_BOARD_DIR)/stm32n6xx_nucleo.c \
                        $(PAYLOAD_BOARD_DIR)/stm32n6xx_nucleo_bus.c \
                        $(PAYLOAD_BOARD_DIR)/stm32n6xx_nucleo_xspi.c \
                        $(PAYLOAD_BSP)/Components/mx25um51245g/mx25um51245g.c
else ifeq ($(PAYLOAD_BOARD),STM32N6570-DK)
PAYLOAD_CPPFLAGS += -DSTM32N6570_DK_REV
PAYLOAD_LINKER = $(PAYLOAD_APP)/stm32n657_dk.ld
PAYLOAD_BOARD_DIR = $(PAYLOAD_BSP)/STM32N6570-DK
PAYLOAD_BOARD_SOURCES = $(PAYLOAD_BOARD_DIR)/stm32n6570_discovery.c \
                        $(PAYLOAD_BOARD_DIR)/stm32n6570_discovery_bus.c \
                        $(PAYLOAD_BOARD_DIR)/stm32n6570_discovery_lcd.c \
                        $(PAYLOAD_BOARD_DIR)/stm32n6570_discovery_xspi.c \
                        $(PAYLOAD_BSP)/Components/aps256xx/aps256xx.c \
                        $(PAYLOAD_BSP)/Components/mx66uw1g45g/mx66uw1g45g.c
else
$(error Unsupported PAYLOAD_BOARD '$(PAYLOAD_BOARD)'; use NUCLEO-N657X0-Q or STM32N6570-DK)
endif

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
                   $(PAYLOAD_BOARD_DIR) $(PAYLOAD_BSP)/Components/Common \
                   $(PAYLOAD_BSP)/Components/aps256xx \
                   $(PAYLOAD_RTOS)/include $(PAYLOAD_RTOS_PORT) \
                   $(PAYLOAD_CAMERA) $(PAYLOAD_CAMERA)/sensors \
                   $(PAYLOAD_CAMERA)/sensors/imx335 \
                   $(PAYLOAD_CAMERA)/sensors/vd55g1 \
                   $(PAYLOAD_CAMERA)/sensors/vd6g \
                   $(PAYLOAD_CAMERA)/sensors/vd1943 $(PAYLOAD_ISP)/Inc \
                   src/drivers/fatfs $(PAYLOAD_UVCL)/Inc $(PAYLOAD_UVCL)/Src \
                   $(PAYLOAD_UVCL)/Inc/usbx $(PAYLOAD_UVCL)/Src/usbx \
                   $(PAYLOAD_USBX)/common/core/inc $(PAYLOAD_USBX)/ports/generic/inc \
                   $(PAYLOAD_USBX)/common/usbx_device_classes/inc \
                   $(PAYLOAD_USBX)/common/usbx_stm32_device_controllers

PAYLOAD_SOURCES = $(wildcard $(PAYLOAD_APP)/*.c $(PAYLOAD_DRIVERS)/*.c) \
                  $(wildcard $(PAYLOAD_HAL)/Src/*.c) $(PAYLOAD_BOARD_SOURCES) \
                  $(PAYLOAD_DEVICE)/Source/Templates/system_stm32n6xx_fsbl.c \
                  $(PAYLOAD_CAMERA)/cmw_camera.c $(PAYLOAD_CAMERA)/cmw_utils.c \
                  $(PAYLOAD_CAMERA)/sensors/cmw_imx335.c \
                  $(PAYLOAD_CAMERA)/sensors/imx335/imx335.c \
                  $(PAYLOAD_CAMERA)/sensors/imx335/imx335_reg.c \
                  $(PAYLOAD_CAMERA)/sensors/cmw_vd55g1.c \
                  $(PAYLOAD_CAMERA)/sensors/vd55g1/vd55g1.c \
                  $(PAYLOAD_CAMERA)/sensors/cmw_vd66gy.c \
                  $(PAYLOAD_CAMERA)/sensors/vd6g/vd6g.c \
                  $(PAYLOAD_CAMERA)/sensors/cmw_vd1943.c \
                  $(PAYLOAD_CAMERA)/sensors/vd1943/vd1943.c \
                  $(filter-out %/isp_conf_template.c,$(wildcard $(PAYLOAD_ISP)/Src/*.c)) \
                  $(PAYLOAD_RTOS)/croutine.c $(PAYLOAD_RTOS)/event_groups.c \
                  $(PAYLOAD_RTOS)/list.c $(PAYLOAD_RTOS)/queue.c \
                  $(PAYLOAD_RTOS)/tasks.c $(PAYLOAD_RTOS)/timers.c \
                  $(PAYLOAD_RTOS_PORT)/port.c $(PAYLOAD_RTOS_PORT)/portasm.c \
                  src/drivers/fatfs/ff.c $(PAYLOAD_UVCL)/Src/uvcl.c \
                  $(PAYLOAD_UVCL)/Src/uvcl_desc.c $(PAYLOAD_UVCL)/Src/usbx/uvcl_usbx.c \
                  $(wildcard $(PAYLOAD_USBX)/common/core/src/*.c) \
                  $(wildcard $(PAYLOAD_USBX)/common/usbx_stm32_device_controllers/*.c) \
                  $(wildcard $(PAYLOAD_USBX)/common/usbx_device_classes/src/*.c)
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
