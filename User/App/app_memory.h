/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_memory.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_MEMORY_H
#define APP_MEMORY_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

#include "drv_flash.h"
#include "lib_comm.h"

#define MEMORY_OS_DELAY  30

#define MEMORY_DEV_INFO_SECTOR  0
#define MEMORY_PROG_NUM     1024 // sectors
#define MEMORY_PROG_SIZE    4096 // byte
#define CPR_ACTIVE_SECTOR_NUM 4  
#define DATA_BLOCK_SIZE     16   // 统一数据块大小


typedef enum
{
    UPLOAD_NONE  = 0,
	UPLOAD_LOG   = 1,
    UPLOAD_HISTORY = 2,
    
}CPR_Upload_Method_EnumDef;

typedef struct {
    bool SendBusyFlag;
    bool UpdateStatus;
    uint8_t Status;        // true: start sending, false: stop sending
    CPR_Upload_Method_EnumDef Upload;//UPLOAD_NONE;
    uint8_t SendBuffer[128];
    uint16_t Index;
} Memory_SendState_TypeDef;


typedef enum {
    MEM_FACTORY_START = 0,
    MEM_FACTORY_END   = 0,
    MEM_VOLUME        = 3,
    MEM_METRONOME     = 4,
    MEM_WIFI          = 6,
    MEM_SECRET        = 7,
    MEM_UTC           = 8,
    MEM_CPR_START     = 128,
    MEM_CPR_END       = 176,
    MEM_LOG_START     = 528,
    MEM_LOG_END       = 576,
} Memory_Area_EnumDef;

typedef enum {
    TYPE_HC600_N = 0,  // comm
    TYPE_HC610_A,       // Audio
    TYPE_HC620_B,       // ble 
    TYPE_HC630_AB,      // audio ble 
    TYPE_HCTEST_ABC,    // audio ble 
    TYPE_MAX,
} Device_Type_EnumDef;

typedef enum {
    History_Data_Send_Idle,
    History_Data_Send_Start,
    History_Data_Sending,
    History_Data_Send_End,  
} Memory_Send_History_EnumDef;

typedef struct {
    uint8_t Cell_Status;
    uint8_t IFC_Status;
    uint8_t IFC_CODE;
    uint8_t None_Count;
    uint8_t Err_Count;
} Memory_IFC_TypeDef;

typedef enum {
    DEV_FACTORY_INFO = 0xA0,
    DEV_SECTOR_INFO = 0xA1,
    TIMESTAMP_DATA = 0xA2,
    CPR_DATA = 0xA3,
    IFC_DATA = 0xA4,
    VOLUME_DATA = 0xA5,
    BOOT_TIMESTAMP = 0xA6,
    DEV_LOG = 0xA7,
    SELF_CHECK = 0xA8,
    METRONOME_DATA = 0xA9,
    WIFI_DATA = 0xAA,
    DATA_ERR = 0xEE,
    DATA_EMPTY = 0xFF,
} ENUM_MEM_DATA_TYPE;

typedef struct {
    ENUM_MEM_DATA_TYPE type;
    uint8_t Device_Type;
    char Device_SN[13];
    uint8_t Reserve[7];
    uint16_t CheckSum;
} Memory_Block_Factory_TypeDef;

typedef struct {
    ENUM_MEM_DATA_TYPE type;
    uint8_t Reserve1[3];    
    uint32_t erase_count;
    uint16_t CheckSum;
    uint8_t  Area_Status;
    uint8_t Reserve2[1];
    uint32_t cell_Contain_Status; // Status of each cell in the sector 
} Memory_Block_Sector_TypeDef;

typedef struct {    
    Device_Type_EnumDef Type;
    char Name[6];
    char Device_SN[14]; 
} Factory_TypeDef;

typedef enum {
    AREA_ACTIVE = 0xAF, 
    AREA_FREE = 0xA5,  
    AREA_DIRTY = 0xA0,
} Area_Status_EnumDef;

// 单区块存储结构
typedef struct {
    uint32_t sector;
    uint16_t data_size;
    void* data_ptr;
} SingleSectorStorage;

typedef struct 
{
    uint8_t type;
    uint8_t Reserved[13];
    uint16_t CheckSum;
}Gen_Data_Typedef;

typedef struct {
    uint8_t Volume_Mem[8];
    uint8_t Metronome_Mem[8];
    uint8_t Wifi_Mem[64];
    uint8_t Secret_Mem[34];
    uint8_t UTC_Offset_Time[10];
} SingleStorageData_TypeDef;

// 环形缓冲区存储结构
typedef struct {
    uint32_t start_sector;
    uint32_t end_sector;
    uint32_t current_address;
    uint32_t read_address;
    uint32_t sector_group_size;
	Memory_Block_Sector_TypeDef current_sector_info;
} RingBufferStorage;

// 公共函数声明
void App_Memory_init(void);
void App_Memory_Handle(void);

// 单区块存储API
bool SingleSector_Init(SingleSectorStorage* storage);
bool SingleSector_Read(SingleSectorStorage* storage);
bool SingleSector_Write(SingleSectorStorage* storage);
Factory_TypeDef* App_Memory_GetFactoryInfo(void);


#ifdef __cplusplus
}
#endif
#endif  // APP_MEMORY_H
