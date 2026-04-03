/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file lib_comm.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef LIB_COMM_H
#define LIB_COMM_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

#include "main.h"
#include "app_memory.h"

typedef struct 
{
    uint8_t Cmd; // 0x01 写  0x02 读
    uint16_t Strength;
    uint16_t Freq;
}Comm_CPR_Data_Typedef;


typedef struct 
{
    uint32_t Addr;
    uint8_t Cmd; // 0x01 写  0x02 读
    uint8_t  length;
    uint8_t  Data[16];
}Comm_Flash_Data_Typedef;

typedef enum
{
	E_COMM_HANDSHAKE        = 0x01,
    E_COMM_DEV_INFO         = 0x02,
    E_COMM_HEARTBEAT        = 0x03,
    E_COMM_VERSION			= 0x04,
    E_COMM_FLASH            = 0x05,

    E_COMM_WARNING          = 0x0E,
    E_COMM_ERR              = 0x0F, 
    
    E_COMM_CPR              = 0x10,

}Communication_CMD_EnumDef;

typedef enum
{
    E_CMD_HANDSHAKE         = 0x01,
    E_CMD_HEARTBEAT         = 0x03,
    E_CMD_DISCONNECT        = 0x04,
    E_CMD_SELF_CHECK        = 0x05,

    E_CMD_DEV_INFO          = 0x11,
    E_CMD_BLE_INFO          = 0x13,
    E_CMD_WIFI_SETTING      = 0x14,
    E_CMD_COMM_SETTING      = 0x15,
    E_CMD_TCP_SETTING       = 0x16,

    E_CMD_UPLOAD_METHOD     = 0x30,
    E_CMD_CPR_DATA          = 0x31,
    E_CMD_TIME_SYCN         = 0x33,
    E_CMD_BATTERY           = 0x34,
    E_CMD_LANGUAGE          = 0x35,
    E_CMD_VOLUME            = 0x36,
    E_CMD_CPR_RAW_DATA      = 0x37,
    E_CMD_CLEAR_MEMORY      = 0x38,
    E_CMD_BOOT_TIME         = 0x39,
    E_CMD_METRONOME         = 0x3A,
    E_CMD_UTC_SETTING       = 0x3B,
    E_CMD_LOG_DATA          = 0x40,
    E_CMD_HISTORY_DATA      = 0x41,

    E_CMD_OTA_REQUEST       = 0xEA,
    E_CMD_OTA_FILE_INFO     = 0xEB,
    E_CMD_OTA_OFFSET        = 0xEC,
    E_CMD_OTA_DATA          = 0xED,
    E_CMD_OTA_RESULT        = 0xEE,
}Wireless_Key_EnumDef;

typedef struct {
    uint8_t Soft;    	
    uint8_t Build;
    uint8_t Hard; 
} Version_TypeDef;

typedef enum
{
    FLASH_READ,
	FLASH_WRITE, 
    FLASH_WRITE_PCR,
}COM_FLASH_0x05_EnumDef;

typedef enum
{
    PCR_WRITE,
    PCR_READ_REALTIME,
    PCR_READ_HISTORY,
}COM_PCR_0x10_EnumDef;

uint16_t Crc16Compute(const uint8_t *Data, uint16_t Length);

void Communication_Process(void);

#ifdef __cplusplus
}
#endif
#endif  // LIB_COMM_H
/**************************End of file********************************/

