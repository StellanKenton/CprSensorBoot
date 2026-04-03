/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_blemodule.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef DRV_BLEMODULE_H
#define DRV_BLEMODULE_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#include "stm32f1xx_hal.h"
#include "lib_ringbuffer.h"
#include "usart.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

#define BLE_MOUDLES_MAX_LEN 256
#define RING_BLE_BUFFSIZE 1024
#define RX_BUFFER_SIZE 512

#define BLE_CR  0x0D    // 或 '\r'
#define BLE_LF  0x0A    // 或 '\n'
#define WIRELESS_MAX_LEN 512

typedef enum {
    DMA_TX_IDLE,
    DMA_TX_BUSY,
    DMA_TX_COMPLETE,
    DMA_TX_ERROR
} DMA_TX_Status;

typedef enum
{
    wRecv_None = 0x00,
    wRecv_Data = 0x01,
    wRecv_Char = 0x02,
    wRecv_Puls = 0x03,
}WireLess_Recv_EnumDef;


void WirelessComm_DMA_Recive(void);
DMA_TX_Status Get_DMA_TX_Status(UART_HandleTypeDef *huart);
void Wireless_Gpio_Init(void);
void Wireless_Reset_Enable(void);
void Wireless_Reset_Disable(void);
void Wireless_Uart_Send(const uint8_t *pData, uint16_t Size);
WireLess_Recv_EnumDef WireLess_Modlue_AT_Check(CBuff *Ring_Comm,char *cmd, uint16_t wait_time,WireLess_Recv_EnumDef out);
void App_Module_Reset(void);
uint8_t App_Ble_Init(void);
uint8_t *Drv_GetBleMacAddress(void);
uint8_t Wireless_Handle_Data(CBuff *Ring_Comm);
uint8_t hexCharToValue(char c);
void HexChangeChar(uint8_t val, uint8_t *out1, uint8_t *out2);
UART_HandleTypeDef * Drv_BleModuleGetUart(void);
#ifdef __cplusplus
}
#endif
#endif  // EXAMPLE_H
/**************************End of file********************************/

