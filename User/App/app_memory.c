/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_memory.c
* \brief Implementation of memory management functions.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_memory.h"
#include "app_system.h"
#include "log.h"
#include "stdlib.h"
// 全局变量
Factory_TypeDef Factory_Info;
uint8_t Mem_Clear_Flag = 0;
uint8_t Wifi_Save_Flag = 0;
    
volatile Memory_SendState_TypeDef Memory_SendState;

static Memory_Block_Factory_TypeDef Dev_Factory_Info;
static volatile uint8_t read_buffer[128];
static volatile uint8_t Mem_Write_Buffer[64];
static SingleStorageData_TypeDef SingleStorageData;

// 单区块存储实例
SingleSectorStorage FactoryStorage = {
    .sector = MEM_FACTORY_START,
    .data_size = sizeof(Memory_Block_Factory_TypeDef),
    .data_ptr = &Dev_Factory_Info
};

SingleSectorStorage VolumeStorage = {
    .sector = MEM_VOLUME,
    .data_size = 8,
    .data_ptr = &SingleStorageData.Volume_Mem
};

SingleSectorStorage MetronomeStorage = {
    .sector = MEM_METRONOME,
    .data_size = 8,
    .data_ptr = &SingleStorageData.Metronome_Mem
};

SingleSectorStorage WifiStorage = {
    .sector = MEM_WIFI,
    .data_size = 64,
    .data_ptr = &SingleStorageData.Wifi_Mem
};

SingleSectorStorage SecretStorage = {
    .sector = MEM_SECRET,
    .data_size = 34,
    .data_ptr = &SingleStorageData.Secret_Mem
};

SingleSectorStorage UTCTimeStorage = {
    .sector = MEM_UTC,
    .data_size = 10,
    .data_ptr = &SingleStorageData.UTC_Offset_Time
};
// 环形缓冲区实例
RingBufferStorage CPRBuffer = {
    .start_sector = MEM_CPR_START,
    .end_sector = MEM_CPR_END,
    .sector_group_size = CPR_ACTIVE_SECTOR_NUM
};

RingBufferStorage LogBuffer = {
    .start_sector = MEM_LOG_START,
    .end_sector = MEM_LOG_END,
    .sector_group_size = 4  // 根据实际需求设置
};

/*!
* \brief 检查地址是否跨页
* \param   start_addr: 起始地址
*          length: 数据长度
* \return 1: 跨页 0: 不跨页
*/
uint8_t is_cross_page(uint32_t start_addr, uint16_t length) {
    uint32_t page_mask = 256 - 1;
    return ((start_addr & ~page_mask) != ((start_addr + length - 1) & ~page_mask));
}

/*!
* \brief 获取跨页的两个段长度
* \param   start_addr: 起始地址
*          length: 数据长度
*          first_seg_len: 第一段长度指针
*          second_seg_len: 第二段长度指针
* \return none
*/
void get_cross_page_segments(uint32_t start_addr, uint16_t length, 
                           uint16_t *first_seg_len, uint16_t *second_seg_len) {
    uint32_t remaining_in_first_page = 256 - (start_addr & 255);
    *first_seg_len = (length <= remaining_in_first_page) ? length : remaining_in_first_page;
    *second_seg_len = length - *first_seg_len;
}

/*!
* \brief 写入数据到Flash，处理跨页情况
* \param   addr: 写入地址
*          txdata: 待写入数据指针
*          len: 数据长度
* \return none
*/
void App_Memory_WritePage(uint32_t addr, volatile uint8_t *txdata, uint16_t len) {
    uint16_t first_seg_len, second_seg_len;
    if(is_cross_page(addr, len) == 0) {
        Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr, (uint8_t *)txdata, len);
    } else {     
        get_cross_page_segments(addr, len, &first_seg_len, &second_seg_len);
        Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr, (uint8_t *)txdata, first_seg_len);
        HAL_Delay(5);
        Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr + first_seg_len, (uint8_t *)(txdata + first_seg_len), second_seg_len);
    }
    HAL_Delay(10);
}

/*!
* \brief 初始化单区块存储
* \param   storage: 存储结构指针
* \return true: 成功 false: 失败
*/
bool SingleSector_Init(SingleSectorStorage* storage) {
    if (storage == NULL || storage->data_ptr == NULL) return false;
    
    uint8_t retry_count = 0;
    bool crc_valid = false;
    uint16_t Calculated_CheckSum;
    
    do {
        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, (storage->sector << 12), 
                                (uint8_t *)storage->data_ptr, storage->data_size) != HAL_OK) {
            retry_count++;
            HAL_Delay(10);
            continue;
        }
        
        Calculated_CheckSum = Crc16Compute((const uint8_t*)storage->data_ptr, storage->data_size - 2);
        uint16_t stored_crc = *((uint16_t*)((uint8_t*)storage->data_ptr + storage->data_size - 2));
        
        if (Calculated_CheckSum == stored_crc) {
            crc_valid = true;
            break;
        }
        
        retry_count++;
        HAL_Delay(10);
    } while (retry_count < 10);
    
    return crc_valid;
}

/*!
* \brief 读取单区块数据
* \param   storage: 存储结构指针
* \return true: 成功 false: 失败
*/
bool SingleSector_Read(SingleSectorStorage* storage) {
    if (storage == NULL || storage->data_ptr == NULL) return false;
    
    for (uint8_t i = 0; i < 3; i++) {
        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, (storage->sector << 12),
                                  (uint8_t *)storage->data_ptr, storage->data_size) == HAL_OK) {
            // 验证CRC
            uint16_t Calculated_CheckSum = Crc16Compute((const uint8_t*)storage->data_ptr, storage->data_size - 2);
            uint16_t stored_crc = *((uint16_t*)((uint8_t*)storage->data_ptr + storage->data_size - 2));

            if (Calculated_CheckSum == stored_crc) {
                return true;
            }
        }
        HAL_Delay(10);
    }
    return false;
}

/*!
* \brief 写入单区块数据
* \param   storage: 存储结构指针
* \return true: 成功 false: 失败
*/
bool SingleSector_Write(SingleSectorStorage* storage) {
    if (storage == NULL || storage->data_ptr == NULL) return false;
    
    // 计算并存储CRC
    uint16_t Checksum = Crc16Compute((const uint8_t*)storage->data_ptr, storage->data_size - 2);
    *((uint16_t*)((uint8_t*)storage->data_ptr + storage->data_size - 2)) = Checksum;
    
    // 擦除扇区
    Drv_GD25Q32_EraseSector(&GD25Q32_Dev, (storage->sector << 12));
    HAL_Delay(200);
    
    // 写入数据
    App_Memory_WritePage((storage->sector << 12), (uint8_t *)storage->data_ptr, storage->data_size);
    return true;
}


void Memory_LogHandle(char *data)
{
    uint32_t ReadAddr;
    switch(data[0]) {
        case 'r':
            ReadAddr = 0;
            for(int i=1; i<9; i++)
            {
                if(data[i+1] >= '0' && data[i+1] <= '9')
                    ReadAddr = (ReadAddr << 4) + (data[i+1] - '0');
                else if(data[i+1] >= 'A' && data[i+1] <= 'F')
                    ReadAddr = (ReadAddr << 4) + (data[i+1] - 'A' + 10);
                else if(data[i+1] >= 'a' && data[i+1] <= 'f')
                    ReadAddr = (ReadAddr << 4) + (data[i+1] - 'a' + 10);
            }
            Drv_GD25Q32_ReadPage(&GD25Q32_Dev, ReadAddr, (uint8_t *)read_buffer, 16);
            LOG_I("Flash Read @0x%08X: ", ReadAddr);
            LOG_I("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", read_buffer[0], read_buffer[1], read_buffer[2], read_buffer[3],
                                           read_buffer[4], read_buffer[5], read_buffer[6], read_buffer[7], read_buffer[8], read_buffer[9], read_buffer[10], read_buffer[11], read_buffer[12], read_buffer[13], read_buffer[14], read_buffer[15]);
            LOG_I("\r\n");
            break;
        case 'w':
            break;
        default:
            break;
    }
}

/*!
* \brief 初始化存储模块
* \param   none
* \return none
*/
void App_Memory_init(void) {

    //App_Memory_Write_Test_Dev();
    Memory_SendState.Upload = UPLOAD_NONE;
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4,GPIO_PIN_SET);
    // 初始化单区块存储
    if (SingleSector_Init(&FactoryStorage)) {
        Factory_Info.Type = (Device_Type_EnumDef)Dev_Factory_Info.Device_Type;
        memcpy(Factory_Info.Device_SN, Dev_Factory_Info.Device_SN, 13);
        
        switch(Factory_Info.Type) {
            case TYPE_HC600_N:      memcpy(Factory_Info.Name, "HC600", 6); break;
            case TYPE_HC610_A:      memcpy(Factory_Info.Name, "HC610", 6); break;
            case TYPE_HC620_B:      memcpy(Factory_Info.Name, "HC620", 6); break;
            case TYPE_HC630_AB:     memcpy(Factory_Info.Name, "HC630", 6); break;
            case TYPE_HCTEST_ABC:   memcpy(Factory_Info.Name, "HCtest", 6); break;
            default:                memcpy(Factory_Info.Name, "HC6XX", 6); break; 
        }      
    }
    LOG_I("[info] Device Type: %s, SN: %s\r\n", Factory_Info.Name, Factory_Info.Device_SN);
    Log_RegisterFunction("mem", Memory_LogHandle);
}

Factory_TypeDef* App_Memory_GetFactoryInfo() {
    return &Factory_Info;
}

/*!
* \brief 处理存储模块任务
* \param   none
* \return none
*/
void App_Memory_Handle() {
    
}


/**************************End of file********************************/
