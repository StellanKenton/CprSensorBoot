/************************************************************************************
* @file     : bspmcuflash.h
* @brief    : STM32 internal flash BSP adapter for drvmcuflash.
* @details  : Provides page erase, half-word program, and page information hooks
*             for the reusable MCU flash driver core.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef BSPMCUFLASH_H
#define BSPMCUFLASH_H

#include <stdint.h>

#include "../../rep/driver/drvmcuflash/drvmcuflash.h"

#ifdef __cplusplus
extern "C" {
#endif

eDrvStatus bspMcuFlashInit(void);
eDrvStatus bspMcuFlashUnlock(void);
eDrvStatus bspMcuFlashLock(void);
eDrvStatus bspMcuFlashEraseSector(uint32_t sectorIndex);
eDrvStatus bspMcuFlashProgram(uint32_t address, const uint8_t *buffer, uint32_t length);
eDrvStatus bspMcuFlashGetSectorInfo(uint32_t address, uint32_t *sectorIndex, uint32_t *sectorStart, uint32_t *sectorSize);

#ifdef __cplusplus
}
#endif

#endif  // BSPMCUFLASH_H
/**************************End of file********************************/
