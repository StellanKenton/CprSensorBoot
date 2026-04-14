/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_i2c.h
* \brief I2C slave implementation with enhanced diagnostics.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_IIC_H
#define APP_IIC_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Public defines -----------------------------------------------------------*/
#define SW_SLAVE_ADDR         0x1E  // 7-bit: 0x1E
#define SW_I2C_ID             0xF1  // Device ID code

// Command definitions
typedef enum {
    CMD_GET_DEVICE_ID        = 0x0F,  // 设备ID获取
    CMD_QUERY_READABLE_DATA  = 0xA0,  // 可读取数据查询
    CMD_GET_MAC_ADDRESS      = 0xA1,  // MAC地址获取
    CMD_GET_SERIAL_NUMBER    = 0xA2,  // 序列号获取
    CMD_GET_PRESS_DATA       = 0xA3,  // 按压数据获取
    CMD_GET_SELF_TEST_RESULT = 0xA4,  // 自检结果获取
    CMD_GET_DEVICE_INFO      = 0xA5   // 设备信息获取
} DeviceCommand;

/* Type definitions ---------------------------------------------------------*/
typedef enum {
    I2C_ERROR_BUS = 0,           // Bus error
    I2C_ERROR_COMM_ERROR,        // Communication error
    I2C_ERROR_ACK,               // ACK failure
    I2C_ERROR_TIMEOUT,           // Timeout error
    I2C_ERROR_INIT,              // Initialization error
    I2C_ERROR_INVALID_CMD,       // Invalid command received
    I2C_ERROR_BUFFER_UNDERFLOW,  // Buffer underflow
    I2C_ERROR_COMM_FAULT,        // Communication fault (after retries)
    
    I2C_ERROR_COUNT              // Total number of error codes (keep last)
} I2C_ErrorCode;

typedef union {
    uint16_t all;
    struct {
        uint16_t bus_error          : 1;
        uint16_t Comm_Error         : 1;
        uint16_t ack_failure        : 1;
        uint16_t timeout            : 1;
        uint16_t invalid_command    : 1;
        uint16_t buffer_underflow   : 1;
        uint16_t communication_fault: 1;
		uint16_t int_error			: 1;
        uint16_t reserved           : 6;
    } flags;
} I2C_ErrorStatusType;

typedef struct {
    I2C_HandleTypeDef *i2cDev;
    uint8_t slaveAddr;
    uint8_t rxBuffer[16];
    uint8_t txBuffer[16];
    I2C_ErrorStatusType errorStatus;
    uint8_t retryCount;
} I2C_Slave_TypeDef;

typedef struct {
    bool connected;
    uint16_t Ticks;
} I2C_Connection_TypeDef;

typedef struct {
    uint8_t SNnumber[16];
    uint8_t SelfCheck[16];
    uint8_t DevInfo[16];
} I2C_EncryData_TypeDef;

/* Public variables ---------------------------------------------------------*/
extern I2C_Slave_TypeDef SwI2CDev;

/* Function prototypes ------------------------------------------------------*/
HAL_StatusTypeDef SlaveI2C_Init(void);
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c);
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode);
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c);
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c);
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);
bool I2C_GetConnectionStatus(void);
void I2C_Loop_Process(void);
#ifdef __cplusplus
}
#endif
#endif  // APP_IIC_H
/**************************End of file********************************/
