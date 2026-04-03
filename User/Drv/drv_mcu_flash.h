#ifndef __DRV_MCU_FLASH_H
#define __DRV_MCU_FLASH_H

#include "main.h"

// Return codes
#define MCU_FLASH_OK           0
#define MCU_FLASH_ERROR        -1
#define MCU_FLASH_TIMEOUT      -2
#define MCU_FLASH_INV_ADDR     -3

// Chinese：STM32分为256页，每页的大小是2kb，因此总大小为512kb，页是最小的擦除单位
// Chinese: 请注意区分boot的分区时，boot区和app区的页需要连续编号的，不要页起始地址和分区起始地址混淆
// english：STM32 is divided into 256 pages, each page is 2kb, so the total size is 512kb, and the page is the smallest erase unit.


// Flash Constants
#define MCU_FLASH_START_ADDR   0x08000000
// STM32F103xE is High Density, Page Size is 2KB (0x800)
#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE        0x800
#endif
#define MCU_FLASH_PAGE_SIZE    FLASH_PAGE_SIZE

// Function Prototypes
int8_t Drv_McuFlash_Init(void);
int8_t Drv_McuFlash_ErasePage(uint32_t addr);
int8_t Drv_McuFlash_Write(uint32_t addr, const uint8_t *pData, uint32_t len);
int8_t Drv_McuFlash_Read(uint32_t addr, uint8_t *pData, uint32_t len);
int8_t Drv_ExtFlash_ErasePage(uint32_t addr);
int8_t Drv_ExtFlash_Write(uint32_t addr, const uint8_t *pData, uint32_t len);
int8_t Drv_ExtFlash_Read(uint32_t addr, uint8_t *pData, uint32_t len);
#endif // __DRV_MCU_FLASH_H
