#include "drv_mcu_flash.h"
#include <string.h>

/**
  * @brief  Initialize MCU Flash Driver
  * @retval 0 on success, <0 on failure
  */
int8_t Drv_McuFlash_Init(void)
{
    // Unlock happens before operations usually, but we can ensure it's locked initally
    HAL_FLASH_Lock();
    return MCU_FLASH_OK;
}

/**
  * @brief  Erase Flash area
  * @param  addr: Start address (must be page aligned)
  * @retval 0 on success, <0 on failure
  */
int8_t Drv_McuFlash_ErasePage(uint32_t addr)
{
    if (addr < MCU_FLASH_START_ADDR) return MCU_FLASH_INV_ADDR;
    
    // Calculate start and end pages
    // Note: addr should ideally be page-aligned to avoid accidental erasure of preceding data in the same page.
    uint32_t startPageStrAddr = (addr - MCU_FLASH_START_ADDR) / MCU_FLASH_PAGE_SIZE * MCU_FLASH_PAGE_SIZE + MCU_FLASH_START_ADDR;

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = startPageStrAddr;
    EraseInitStruct.NbPages     = 1;
    EraseInitStruct.Banks       = FLASH_BANK_1; 

    HAL_FLASH_Unlock();

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return MCU_FLASH_ERROR;
    }

    HAL_FLASH_Lock();
    return MCU_FLASH_OK;
}

/**
  * @brief  Write data to Flash
  * @param  addr: Start address
  * @param  pData: Pointer to data buffer
  * @param  len: Length in bytes
  * @retval 0 on success, <0 on failure
  */
int8_t Drv_McuFlash_Write(uint32_t addr, const uint8_t *pData, uint32_t len)
{
    
    // Check alignment if necessary, STM32F1 supports half-word (16-bit) or word (32-bit) programming
    // Here we will handle byte-by-byte logic but underlying must be 16-bit or 32-bit aligned usually.
    // For simplicity, we assume we write in Words (32-bit) for efficiency, pad if needed or handle 16-bit.
    // STM32F1 programming is typically by Half-Word (16-bit).
    __disable_irq();
    HAL_FLASH_Unlock();

    uint32_t i;
    for (i = 0; i < len; i += 2) {
        uint16_t data = 0xFFFF;
        
        if (i + 1 < len) {
            data = pData[i] | (pData[i+1] << 8);
        } else {
            // Last byte, pad with 0xFF
            data = pData[i] | (0xFF << 8); 
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, data) != HAL_OK) {
            HAL_FLASH_Lock();
            return MCU_FLASH_ERROR;
        }
    }

    HAL_FLASH_Lock();
    __enable_irq();
    return MCU_FLASH_OK;
}

/**
  * @brief  Read data from Flash
  * @param  addr: Start address
  * @param  pData: Pointer to buffer
  * @param  len: Length in bytes
  * @retval 0 on success, <0 on failure
  */
int8_t Drv_McuFlash_Read(uint32_t addr, uint8_t *pData, uint32_t len)
{
    // Direct memory access
    memcpy(pData, (void*)addr, len);
    return MCU_FLASH_OK;
}

#include "drv_flash.h"
extern GD25Q32C_Device_TypeDef GD25Q32_Dev;

int8_t Drv_ExtFlash_ErasePage(uint32_t addr)
{
    Drv_GD25Q32_EraseSector(&GD25Q32_Dev, addr);
    return 0;
}


int8_t Drv_ExtFlash_Write(uint32_t addr, const uint8_t *pData, uint32_t len)
{
    return Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr, (uint8_t *)pData, len);
}

int8_t Drv_ExtFlash_Read(uint32_t addr, uint8_t *pData, uint32_t len)
{
    return Drv_GD25Q32_ReadPage(&GD25Q32_Dev, addr, (uint8_t *)pData, len);
}

