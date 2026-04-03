/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_flash.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "drv_flash.h"


/**************************Init Area*********************************/
GD25Q32C_Device_TypeDef GD25Q32_Dev =
{
    .cs_port = GPIOA, 
    .cs_pin = GPIO_PIN_4,
    .hspi = &hspi1,
};

/*******************************************************************/
static void Drv_GD25Q32_CS_Low(GD25Q32C_Device_TypeDef *hgd);
static void Drv_GD25Q32_CS_High(GD25Q32C_Device_TypeDef *hgd);
//static uint8_t Drv_GD25Q32_Read_Status(GD25Q32C_Device_TypeDef *hgd);
//static void Drv_GD25Q32_Wait_Busy(GD25Q32C_Device_TypeDef *hgd);
static HAL_StatusTypeDef Drv_GD25Q32_Write_Enable(GD25Q32C_Device_TypeDef *device);
// static uint8_t Drv_GD25Q32_ReadByte(GD25Q32C_Device_TypeDef *hgd, uint32_t addr);
// static void Drv_GD25Q32_WriteByte(GD25Q32C_Device_TypeDef *hgd, uint32_t addr, uint8_t data);
//static void Drv_GD25Q32_Write_Disable(GD25Q32C_Device_TypeDef *device);
/**
 * @brief       片选设置低
 * @param       hgd ：MEM设备
 * @retval      无
 */
static void Drv_GD25Q32_CS_Low(GD25Q32C_Device_TypeDef *hgd) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}

/**
 * @brief       片选设置高
 * @param       hgd ：MEM设备
 * @retval      无
 */
static void Drv_GD25Q32_CS_High(GD25Q32C_Device_TypeDef *hgd) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4,GPIO_PIN_SET);
}

/**
 * @brief       写使能
 * @param       hgd ：MEM设备
 * @retval      无
 */
static HAL_StatusTypeDef Drv_GD25Q32_Write_Enable(GD25Q32C_Device_TypeDef *device){
	HAL_StatusTypeDef status = HAL_ERROR;
	uint8_t cmd = GD25Q32_CMD_WRITE_ENABLE;
    Drv_GD25Q32_CS_Low(device);
    status = HAL_SPI_Transmit(device->hspi, &cmd, 1, HAL_MAX_DELAY);
    Drv_GD25Q32_CS_High(device);
	return status;	
}
///**
// * @brief       写失能
// * @param       hgd ：MEM设备
// * @retval      无
// */
//static void Drv_GD25Q32_Write_Disable(GD25Q32C_Device_TypeDef *device){
//    Drv_GD25Q32_SendCmd(device,GD25Q32_CMD_WRITE_DISABLE);
//}
// /**
//  * @brief       读取一个字节
//  * @param       hgd ：MEM设备
//  * @param       addr ：读取的地址
//  * @retval      无
//  */
// static uint8_t Drv_GD25Q32_ReadByte(GD25Q32C_Device_TypeDef *hgd, uint32_t addr) {
//     uint8_t tx_data[4] = {
//         GD25Q32_CMD_READ_DATA,
//         (addr >> 16) & 0xFF,
//         (addr >> 8) & 0xFF,
//         addr & 0xFF
//     };
//     uint8_t rx_data = 0;

//     Drv_GD25Q32_CS_Low(hgd);
//     HAL_SPI_Transmit(hgd->hspi, tx_data, 4, HAL_MAX_DELAY);
//     HAL_SPI_Receive(hgd->hspi, &rx_data, 1, HAL_MAX_DELAY);
//     Drv_GD25Q32_CS_High(hgd);

//     return rx_data;
// }

// /**
//  * @brief       写一个字节
//  * @param       hgd ：MEM设备
//  * @param       addr ：读取的地址
//  * @param       data ：写入的数据
//  * @retval      无
//  */
// static void Drv_GD25Q32_WriteByte(GD25Q32C_Device_TypeDef *hgd, uint32_t addr, uint8_t data) {
//     uint8_t tx_data[5] = {
//         GD25Q32_CMD_PAGE_PROGRAM,
//         (addr >> 16) & 0xFF,
//         (addr >> 8) & 0xFF,
//         addr & 0xFF,
//         data
//     };

//     Drv_GD25Q32_Write_Enable(hgd); // 使能写操作
//     Drv_GD25Q32_CS_Low(hgd);
//     HAL_SPI_Transmit(hgd->hspi, tx_data, 5, HAL_MAX_DELAY);
//     Drv_GD25Q32_CS_High(hgd);

//     Drv_GD25Q32_Wait_Busy(hgd); // 等待写入完成
// }

/**
 * @brief       写一个页
 * @param       hgd ：MEM设备
 * @param       addr ：读取的地址
 * @param       data ：写入的数据
 * @param       len ：写入的数据长度
 * @retval      无
 */
HAL_StatusTypeDef Drv_GD25Q32_WritePage(GD25Q32C_Device_TypeDef *hgd, uint32_t addr, volatile uint8_t *txdata, uint16_t len) {
    uint8_t header[4] = {
        GD25Q32_CMD_PAGE_PROGRAM,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    if (len > 256) len = 256; // 页大小限制
    
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t retry_count = 0;
    
    for (retry_count = 0; retry_count < 3; retry_count++) {
        if (Drv_GD25Q32_Write_Enable(hgd) != HAL_OK) {
            continue;
        }
		Drv_GD25Q32_CS_Low(hgd);
        status = HAL_SPI_Transmit(hgd->hspi, header, 4, HAL_MAX_DELAY);
        if (status != HAL_OK) {
			Drv_GD25Q32_CS_High(hgd);
            continue;
        }
        status = HAL_SPI_Transmit(hgd->hspi, (uint8_t *)txdata, len, HAL_MAX_DELAY);
		Drv_GD25Q32_CS_High(hgd);
        if (status == HAL_OK) {
            break;
        }
    }
    
    if (retry_count >= 3) {
        return HAL_ERROR;
    }
    
    return HAL_OK;
}
/**
 * @brief       擦除sector
 * @param       hgd ：MEM设备
 * @param       addr ：读取的地址
 * @retval      无
 */
HAL_StatusTypeDef Drv_GD25Q32_EraseSector(GD25Q32C_Device_TypeDef *hgd, uint32_t addr) {
    uint8_t header[4] = {
        GD25Q32_CMD_SECTOR_ERASE,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t retry_count = 0;
    
    for (retry_count = 0; retry_count < 3; retry_count++) {
        if (Drv_GD25Q32_Write_Enable(hgd) != HAL_OK) {
            continue;
        }
		Drv_GD25Q32_CS_Low(hgd);
        status = HAL_SPI_Transmit(hgd->hspi, header, 4, HAL_MAX_DELAY);
		Drv_GD25Q32_CS_High(hgd);
        if (status == HAL_OK) {
            break;
        }
    }
    if (retry_count >= 3) {
        return HAL_ERROR;
    }  
    return HAL_OK;
}
/**
 * @brief       读取一个页
 * @param       hgd ：MEM设备
 * @param       addr ：读取的地址
 * @retval      无
 */
HAL_StatusTypeDef Drv_GD25Q32_ReadPage(GD25Q32C_Device_TypeDef *hgd, uint32_t addr, 
                                      volatile uint8_t *rxdata, uint16_t len) 
{
    // 参数校验
    if (hgd == NULL || rxdata == NULL || len == 0) {
        return HAL_ERROR;
    }
    
    if (len > 256) {
        len = 256;
    }
    
    uint8_t tx_buffer[4] = {
        GD25Q32_CMD_READ_DATA,
        (uint8_t)((addr >> 16) & 0xFF),
        (uint8_t)((addr >> 8) & 0xFF),
        (uint8_t)(addr & 0xFF)
    };
    
    HAL_StatusTypeDef status;
    const uint8_t max_retries = 3;
    
    for (uint8_t retry = 0; retry < max_retries; retry++) {
        Drv_GD25Q32_CS_Low(hgd);
        
        // 第一步：发送读命令和地址
        status = HAL_SPI_Transmit(hgd->hspi, tx_buffer, 4, HAL_MAX_DELAY);
        if (status != HAL_OK) {
			Drv_GD25Q32_CS_High(hgd);
            continue; 
        }
        // 第二步：接收数据
        status = HAL_SPI_Receive(hgd->hspi, (uint8_t *)rxdata, len , HAL_MAX_DELAY);
        Drv_GD25Q32_CS_High(hgd);      
        if (status == HAL_OK) {
            return HAL_OK;
        }
    }
    
    return HAL_ERROR;
}

/**
 * @brief       GD25Q32驱动控制
 * @param       device ：MEM设备
 * @param       cmd ：执行的命令
 * @param       arg ：参数
 * @retval      无
 */
int8_t HAL_GD25Q32_Ioctrl(void *device, const uint8_t cmd, void *arg) {
    GD25Q32C_Device_TypeDef *hgd;
    GD25Q32C_WRData_TypeDef *data_pack;
    hgd = (GD25Q32C_Device_TypeDef *)device;
    if (!hgd) {
        return -1; 
    }

    switch((GD25Q32C_Ioctrol_EnumDef)cmd)
    {
        case E_DEVICE_INIT:
            return 1;
        case E_WRITE_MEM:
            data_pack =  (GD25Q32C_WRData_TypeDef *)arg;
            if (!data_pack || !data_pack->data || data_pack->len == 0) {
                return -1;
            }
            Drv_GD25Q32_WritePage(hgd,data_pack->addr,data_pack->data,data_pack->len);
            return 1;
        case E_READ_MEM:
            data_pack =  (GD25Q32C_WRData_TypeDef *)arg;
            if (!data_pack || !data_pack->data || data_pack->len == 0) {
                return -1;
            }          
            Drv_GD25Q32_ReadPage(hgd,data_pack->addr,data_pack->data,data_pack->len);
            return 1;
        case E_ERASE_MEM:
			data_pack =  (GD25Q32C_WRData_TypeDef *)arg;
            if (!data_pack || !data_pack->data || data_pack->len == 0) {
                return -1;
            }          
            Drv_GD25Q32_EraseSector(hgd,data_pack->addr);
            return 1;
        default:
            return -7;
    }
}



/**************************End of file********************************/


