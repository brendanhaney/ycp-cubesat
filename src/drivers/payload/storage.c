/**
******************************************************************************
* @file    main.c
* @author  GPM Application Team
*
******************************************************************************
* @attention
*
* Copyright (c) 2023 STMicroelectronics.
* All rights reserved.
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
*
******************************************************************************
*/

#include <stdint.h>
#include <string.h>

#include "stm32n6xx_hal.h"
#include "ff.h"
#include "diskio.h"
#include "storage.h"

static SD_HandleTypeDef hsd2 = {0};
static uint8_t debug_sd_sector0[512];
static DIR debug_fat_dir;
static FILINFO debug_fat_entry;
static FIL debug_fat_file;
static char debug_fat_file_data[65];

char debug_fat_names[8][13] = {0};

volatile uint32_t debug_sd_init_status = 0xFFFFFFFFU;
volatile uint32_t debug_sd_info_status = 0xFFFFFFFFU;
volatile uint32_t debug_sd_read_status = 0xFFFFFFFFU;
volatile uint32_t debug_sd_read_error = 0xFFFFFFFFU;
/* Legacy names retained for debugger scripts; these report the configured
 * bus-width operation, which now uses the reliable one-bit mode. */
volatile uint32_t debug_sd_4bit_status = 0xFFFFFFFFU;
volatile uint32_t debug_sd_4bit_error = 0xFFFFFFFFU;
volatile uint32_t debug_fat_open_status = 0xFFFFFFFFU;
volatile uint32_t debug_fat_read_status = 0xFFFFFFFFU;
volatile uint32_t debug_fat_close_status = 0xFFFFFFFFU;
volatile uint32_t debug_fat_file_open = 0xFFFFFFFFU;
volatile uint32_t debug_fat_file_read = 0xFFFFFFFFU;
volatile uint32_t debug_fat_file_close = 0xFFFFFFFFU;
volatile uint32_t debug_fat_write_open = 0xFFFFFFFFU;
volatile uint32_t debug_fat_write_result = 0xFFFFFFFFU;
volatile uint32_t debug_fat_write_bytes = 0;
volatile uint32_t debug_fat_write_close = 0xFFFFFFFFU;
volatile uint32_t debug_fat_write_done = 0;
volatile uint32_t debug_fat_file_bytes = 0;
volatile uint32_t debug_fat_entries = 0;
volatile uint32_t debug_sd_error = 0;
volatile uint32_t debug_sd_clock_hz = 0;
volatile uint32_t debug_sd_blocks = 0;
volatile uint32_t debug_sd_block_size = 0;
volatile uint32_t debug_fatfs_writes = 0;
volatile uint32_t debug_fatfs_write_error = 0;
volatile uint32_t debug_fatfs_card_state = 0;

void HAL_SD_MspInit(SD_HandleTypeDef *hsd) {
    if (hsd->Instance != SDMMC2)
        return;

    HAL_PWREx_EnableVddIO5();

    __HAL_RCC_SDMMC2_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF11_SDMMC2;

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOE, &gpio);
}

static void SDMMC2_ReadOnlyTest(void) {
    HAL_SD_CardInfoTypeDef info = {0};

    debug_sd_clock_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC2);

    hsd2.Instance = SDMMC2;
    hsd2.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd2.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd2.Init.BusWide = SDMMC_BUS_WIDE_1B;
    hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    /* Slow the SD clock for the jumper-wired card's signal integrity. */
    hsd2.Init.ClockDiv = 64;

    debug_sd_init_status = HAL_SD_Init(&hsd2);
    debug_sd_error = hsd2.ErrorCode;

    if (debug_sd_init_status == HAL_OK) {
        debug_sd_info_status = HAL_SD_GetCardInfo(&hsd2, &info);

        if (debug_sd_info_status == HAL_OK) {
            debug_sd_blocks = info.LogBlockNbr;
            debug_sd_block_size = info.LogBlockSize;
        }
    }
    if (debug_sd_info_status == HAL_OK) {
        debug_sd_read_status = HAL_SD_ReadBlocks(&hsd2, debug_sd_sector0, 8192, 1, 1000);

        debug_sd_read_error = hsd2.ErrorCode;
    }

    if (debug_sd_info_status == HAL_OK) {
        /* Use the one-bit mode validated with the jumper-wired card. */
        debug_sd_4bit_status = HAL_SD_ConfigWideBusOperation(&hsd2, SDMMC_BUS_WIDE_1B);

        debug_sd_4bit_error = hsd2.ErrorCode;

        // Read again only if the bus configuration succeeded.
        if (debug_sd_4bit_status == HAL_OK) {
            debug_sd_read_status = HAL_SD_ReadBlocks(&hsd2, debug_sd_sector0, 8192, 1, 1000);

            debug_sd_read_error = hsd2.ErrorCode;
        }
    }
}

/* FatFs read-only SDMMC2 adapter */

static FATFS debug_fatfs;
static uint32_t fatfs_sector_buffer[128] __attribute__((aligned(32)));

volatile uint32_t debug_fatfs_mount_status = 0xFFFFFFFFU;
volatile uint32_t debug_fatfs_reads = 0;
volatile uint32_t debug_fatfs_read_error = 0;

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0 || debug_sd_init_status != HAL_OK || debug_sd_4bit_status != HAL_OK)
        return STA_NOINIT;

    return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
    /* SDMMC2 is initialized by SDMMC2_ReadOnlyTest(). */
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    if (pdrv != 0 || buff == NULL || count == 0)
        return RES_PARERR;

    if (disk_status(pdrv) != 0)
        return RES_NOTRDY;

    if (sector >= debug_sd_blocks || count > debug_sd_blocks - sector)
        return RES_PARERR;

    for (UINT i = 0; i < count; i++) {
        HAL_StatusTypeDef result =
            HAL_SD_ReadBlocks(&hsd2, (uint8_t *)fatfs_sector_buffer, sector + i, 1, 1000);

        if (result != HAL_OK) {
            debug_fatfs_read_error = hsd2.ErrorCode;
            return RES_ERROR;
        }

        memcpy(buff + i * 512U, fatfs_sector_buffer, 512U);

        debug_fatfs_reads++;
    }

    return RES_OK;
}

static DRESULT SDMMC2_WaitReady(void) {
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < 3000U) {
        debug_fatfs_card_state = HAL_SD_GetCardState(&hsd2);

        if (debug_fatfs_card_state == HAL_SD_CARD_TRANSFER)
            return RES_OK;

        if (debug_fatfs_card_state == HAL_SD_CARD_ERROR ||
            debug_fatfs_card_state == HAL_SD_CARD_DISCONNECTED)
            return RES_ERROR;
    }

    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    if (pdrv != 0 || buff == NULL || count == 0)
        return RES_PARERR;

    if (disk_status(pdrv) != 0)
        return RES_NOTRDY;

    if (sector >= debug_sd_blocks || count > debug_sd_blocks - sector)
        return RES_PARERR;

    for (UINT i = 0; i < count; i++) {
        /* Copy into our aligned 512-byte buffer */
        memcpy(fatfs_sector_buffer, buff + i * 512U, 512U);

        /* Write one sector to the microSD card */
        if (HAL_SD_WriteBlocks(&hsd2, (const uint8_t *)fatfs_sector_buffer, sector + i, 1, 1000) !=
            HAL_OK) {
            debug_fatfs_write_error = hsd2.ErrorCode;
            return RES_ERROR;
        }

        /* Wait until the card finishes programming */
        if (SDMMC2_WaitReady() != RES_OK) {
            debug_fatfs_write_error = hsd2.ErrorCode;
            return RES_ERROR;
        }

        debug_fatfs_writes++;
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (disk_status(pdrv) != 0)
        return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC:
            return SDMMC2_WaitReady();

        case GET_SECTOR_COUNT:
            if (buff == NULL)
                return RES_PARERR;
            *(DWORD *)buff = debug_sd_blocks;
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (buff == NULL)
                return RES_PARERR;
            *(WORD *)buff = 512;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}

static void FatFs_ReadOnlyMountTest(void) {
    debug_fatfs_mount_status = f_mount(&debug_fatfs, "0:", 1);
}

static void FatFs_ListRootTest(void) {
    if (debug_fatfs_mount_status != FR_OK)
        return;

    debug_fat_open_status = f_opendir(&debug_fat_dir, "0:/");

    if (debug_fat_open_status != FR_OK)
        return;

    for (uint32_t i = 0; i < 8; i++) {
        debug_fat_read_status = f_readdir(&debug_fat_dir, &debug_fat_entry);

        if (debug_fat_read_status != FR_OK)
            break;

        if (debug_fat_entry.fname[0] == '\0')
            break;

        strncpy(debug_fat_names[i], debug_fat_entry.fname, 12);

        debug_fat_names[i][12] = '\0';
        debug_fat_entries++;
    }

    debug_fat_close_status = f_closedir(&debug_fat_dir);
}

static void FatFs_ReadFileTest(void) {
    UINT bytes_read = 0;

    if (debug_fatfs_mount_status != FR_OK)
        return;

    debug_fat_file_open = f_open(&debug_fat_file, "0:/LICENC~1.BRO", FA_READ);

    if (debug_fat_file_open != FR_OK)
        return;

    debug_fat_file_read = f_read(&debug_fat_file, debug_fat_file_data, 64, &bytes_read);

    debug_fat_file_bytes = bytes_read;
    debug_fat_file_data[bytes_read] = '\0';

    debug_fat_file_close = f_close(&debug_fat_file);
}

static void FatFs_WriteTest(void) {
    FIL file;
    UINT bytes_written = 0;

    static const char message[] = "CubeSat STM32N657\r\n"
                                  "SDMMC2 1-bit storage test\r\n";

    /* Create or overwrite our test file */
    debug_fat_write_open = f_open(&file, "0:/CSAT.TXT", FA_CREATE_ALWAYS | FA_WRITE);

    if (debug_fat_write_open != FR_OK) {
        debug_fat_write_done = 1;
        return;
    }

    /* Write text to the file */
    debug_fat_write_result = f_write(&file, message, sizeof(message) - 1, &bytes_written);

    debug_fat_write_bytes = bytes_written;

    /* Close file and commit changes */
    debug_fat_write_close = f_close(&file);

    debug_fat_write_done = 1;
}

void payload_storage_init(void) {
    SDMMC2_ReadOnlyTest();
    FatFs_ReadOnlyMountTest();
    FatFs_ListRootTest();
    FatFs_ReadFileTest();
    FatFs_WriteTest();
}
