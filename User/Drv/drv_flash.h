/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_flash.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef DRV_FLASH_H
#define DRV_FLASH_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

#include "spi.h"


#define GD25Q32_STATUS_BUSY           0x01

#define GD25Q32_CMD_WRITE_ENABLE      0x06
#define GD25Q32_CMD_WRITE_DISABLE     0x04
#define GD25Q32_CMD_READ_STATUS_REG1  0x05
#define GD25Q32_CMD_READ_STATUS_REG2  0x35
#define GD25Q32_CMD_READ_STATUS_REG3  0x15
#define GD25Q32_CMD_PAGE_PROGRAM      0x02
#define GD25Q32_CMD_READ_DATA         0x03
#define GD25Q32_CMD_SECTOR_ERASE      0x20
#define GD25Q32_CMD_CHIP_ERASE        0xC7
#define GD25Q32_CMD_READ_ID           0x9F

typedef struct {
    SPI_HandleTypeDef *hspi;    // SPI句柄
    GPIO_TypeDef *cs_port;      // 片选端口
    uint16_t cs_pin;            // 片选引脚
} GD25Q32C_Device_TypeDef;

typedef struct {
    uint32_t addr;
    uint8_t data[256];
    uint16_t len;
} GD25Q32C_WRData_TypeDef;

typedef enum
{
	E_DEVICE_INIT  = 0,
    E_WRITE_MEM,
    E_READ_MEM,
    E_ERASE_MEM,
    E_MAX,

}GD25Q32C_Ioctrol_EnumDef;

/**************************extern Area*********************************/
extern GD25Q32C_Device_TypeDef GD25Q32_Dev;

int8_t HAL_GD25Q32_Ioctrl(void *device, const uint8_t cmd, void *arg);
HAL_StatusTypeDef Drv_GD25Q32_ReadPage(GD25Q32C_Device_TypeDef *hgd, uint32_t addr,volatile uint8_t *rxdata, uint16_t len);
HAL_StatusTypeDef Drv_GD25Q32_EraseSector(GD25Q32C_Device_TypeDef *hgd, uint32_t addr);
HAL_StatusTypeDef Drv_GD25Q32_WritePage(GD25Q32C_Device_TypeDef *hgd, uint32_t addr, volatile uint8_t *txdata, uint16_t len);
/*******************************************************************/




#ifdef __cplusplus
}
#endif
#endif  // DRV_FLASH_H
/**************************End of file********************************/

