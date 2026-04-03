/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_Wireless.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_WIRELESS_H
#define APP_WIRELESS_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#include "MD5.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif
#include "lib_ringbuffer.h"
#include "stm32f1xx_hal.h"
#include "drv_blemodule.h"

typedef union {
    uint32_t byte;
    struct {
        uint32_t Reply_HandShake : 1;
        uint32_t Reply_Disconnect : 1;
    } bits;
} Ble_Send_ByteUnion;

typedef enum
{
    BLE_HARD_INIT,
	BLE_INIT_STATE,
    BLE_DISCONNECT_STATE,
    BLE_CONNECTED_STATE, 
    BLE_IDLE_STATE,
    BLE_STATE_MAX,
}BLE_FSM_EnumDef;

typedef struct {
    BLE_FSM_EnumDef eState;
    bool IsConnected;
    bool isInited;
    bool needDisconnect;
    bool sendOffline;

} BLE_Mgr_t;

typedef struct {
    bool OTA_Req_BleMaxLen;
    bool OTA_Req_LineMaxLen;
    bool OTA_Req_StartUpdate;
    bool OTA_Req_FileInfo;
    bool OTA_Req_OffsetInfo;
    bool OTA_Req_DataPacket;
    bool OTA_Req_DataPacketErr;

    uint8_t TargetVer[4];
    uint32_t FileSize;
    uint32_t FileCRC32;
    uint32_t OTA_Offset;
    uint16_t OTA_PackSN;
    uint16_t OTA_DataLen;
    uint16_t OTA_PackCRC;
    uint8_t OTA_Data[256];
} Wireless_OTA_RxData_Typedef;

typedef struct {
    bool OTA_Result;
    bool OTA_isAllowed;

    uint16_t OTA_PackMaxLen;
    uint16_t Ble_ModuleMaxLen;
    uint8_t PackDataStatus;
    uint8_t UpdateResult;
    uint8_t Ver[4];
    uint32_t FileOffset;
    
} Wireless_OTA_TxData_Typedef;


void WireLess_Init(void);
void Wireless_ChangeState(BLE_FSM_EnumDef newState);
void WireLess_Process(void);
bool App_Wireless_IsConnected(void);
Wireless_OTA_RxData_Typedef *App_Wireless_Get_OTA_RxData(void);
#ifdef __cplusplus
}
#endif
#endif  // APP_WIRELESS_H@
/**************************End of file********************************/

