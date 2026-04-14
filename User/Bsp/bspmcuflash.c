/***********************************************************************************
* @file     : bspmcuflash.c
* @brief    : STM32 internal flash BSP adapter for drvmcuflash.
* @details  : Converts STM32F1 HAL flash operations to the generic drvmcuflash BSP
*             contract used by the reusable MCU flash driver core.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspmcuflash.h"

#include <stdbool.h>
#include <stddef.h>

#include "main.h"

static eDrvStatus bspMcuFlashMapHalStatus(HAL_StatusTypeDef status);
static bool bspMcuFlashIsValidAddress(uint32_t address);
static bool bspMcuFlashIsValidSectorIndex(uint32_t sectorIndex);

eDrvStatus bspMcuFlashInit(void)
{
    return DRV_STATUS_OK;
}

eDrvStatus bspMcuFlashUnlock(void)
{
    return bspMcuFlashMapHalStatus(HAL_FLASH_Unlock());
}

eDrvStatus bspMcuFlashLock(void)
{
    return bspMcuFlashMapHalStatus(HAL_FLASH_Lock());
}

eDrvStatus bspMcuFlashEraseSector(uint32_t sectorIndex)
{
    FLASH_EraseInitTypeDef lEraseInit = {0};
    uint32_t lPageError = 0U;

    if (!bspMcuFlashIsValidSectorIndex(sectorIndex)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    lEraseInit.PageAddress = FLASH_BASE + (sectorIndex * FLASH_PAGE_SIZE);
    lEraseInit.NbPages = 1U;

    return bspMcuFlashMapHalStatus(HAL_FLASHEx_Erase(&lEraseInit, &lPageError));
}

eDrvStatus bspMcuFlashProgram(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    HAL_StatusTypeDef lHalStatus;
    uint16_t lHalfWord;

    if ((buffer == NULL) || (length == 0U) || ((address & 0x1U) != 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!bspMcuFlashIsValidAddress(address) || !bspMcuFlashIsValidAddress(address + length - 1U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    while (length >= 2U) {
        lHalfWord = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
        lHalStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, lHalfWord);
        if (lHalStatus != HAL_OK) {
            return bspMcuFlashMapHalStatus(lHalStatus);
        }

        address += 2U;
        buffer += 2U;
        length -= 2U;
    }

    if (length == 1U) {
        lHalfWord = (uint16_t)buffer[0] | ((uint16_t)(*(const uint8_t *)(address + 1U)) << 8);
        lHalStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, lHalfWord);
        if (lHalStatus != HAL_OK) {
            return bspMcuFlashMapHalStatus(lHalStatus);
        }
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspMcuFlashGetSectorInfo(uint32_t address, uint32_t *sectorIndex, uint32_t *sectorStart, uint32_t *sectorSize)
{
    uint32_t lIndex;

    if ((sectorIndex == NULL) || (sectorStart == NULL) || (sectorSize == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!bspMcuFlashIsValidAddress(address)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lIndex = (address - FLASH_BASE) / FLASH_PAGE_SIZE;
    *sectorIndex = lIndex;
    *sectorStart = FLASH_BASE + (lIndex * FLASH_PAGE_SIZE);
    *sectorSize = FLASH_PAGE_SIZE;
    return DRV_STATUS_OK;
}

static eDrvStatus bspMcuFlashMapHalStatus(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK:
            return DRV_STATUS_OK;
        case HAL_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        case HAL_BUSY:
            return DRV_STATUS_BUSY;
        case HAL_ERROR:
        default:
            return DRV_STATUS_ERROR;
    }
}

static bool bspMcuFlashIsValidAddress(uint32_t address)
{
    return (address >= FLASH_BASE) && (address <= FLASH_BANK1_END);
}

static bool bspMcuFlashIsValidSectorIndex(uint32_t sectorIndex)
{
    return (FLASH_BASE + ((sectorIndex + 1U) * FLASH_PAGE_SIZE) - 1U) <= FLASH_BANK1_END;
}

/**************************End of file********************************/
