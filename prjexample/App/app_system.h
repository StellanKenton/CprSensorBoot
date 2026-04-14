/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_system.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

#define SoftWare_Version    0x01
#define SoftSub_Version     0x01
#define SoftBuild_Version   0x00

#define HardWare_Version    0x01



#define MODE_CHANGE_BIT (1 << 0)
#define MAX_FAULTS (sizeof(fault_table) / sizeof(fault_table[0]))
#define NO_FAULT_CODE 0  // 无故障时的代码

typedef enum
{
    E_SYSTEM_STANDBY_MODE = 0,
    E_SYSTEM_NORMAL_MODE,
    E_SYSTEM_UPDATE_MODE,
    E_SYSTEM_FACTORY_MODE,

}Systeam_Mode_EnumDef;

typedef union {
    uint8_t byte;  
    struct {
        uint8_t Self_Check_Ok : 1;
        uint8_t MPU_Init_Err : 1;
        uint8_t RTC_Err : 1;
        uint8_t bit3 : 1;
        uint8_t bit4 : 1;
        uint8_t bit5 : 1;
        uint8_t bit6 : 1;
        uint8_t bit7 : 1;
    } bits;
}CPR_ByteUnion;

typedef union {
    uint8_t byte;  
    struct {
        uint8_t Self_Check_Ok : 1;
        uint8_t V3V3_High_Err : 1;
        uint8_t V3V3_Low_Err : 1;
        uint8_t V5V_High_Err : 1;
        uint8_t V5V_Low_Err : 1;
        uint8_t DC_High_Err : 1;
        uint8_t bit6 : 1;
        uint8_t bit7 : 1;
    } bits;
}Power_ByteUnion;

typedef union {
    uint8_t byte;  
    struct {
        uint8_t Self_Check_Ok : 1;
        uint8_t Communication_Err : 1;
        uint8_t Song_Num_Err : 1;
        uint8_t bit3 : 1;
        uint8_t bit4 : 1;
        uint8_t bit5 : 1;
        uint8_t bit6 : 1;
        uint8_t bit7 : 1;
    } bits;
}Audio_ByteUnion;

typedef union {
    uint8_t byte;  
    struct {
        uint8_t Self_Check_Ok : 1;
        uint8_t Init_Err : 1;
        uint8_t bit2 : 1;
        uint8_t bit3 : 1;
        uint8_t bit4 : 1;
        uint8_t bit5 : 1;
        uint8_t bit6 : 1;
        uint8_t bit7 : 1;
    } bits;
}Wireless_ByteUnion;

typedef union {
    uint8_t byte;  
    struct {
        uint8_t Self_Check_Ok : 1;
        uint8_t Init_Err : 1;
        uint8_t bit2 : 1;
        uint8_t bit3 : 1;
        uint8_t bit4 : 1;
        uint8_t bit5 : 1;
        uint8_t bit6 : 1;
        uint8_t bit7 : 1;
    } bits;
}Memory_ByteUnion;

typedef struct 
{
    CPR_ByteUnion Cpr;
    Power_ByteUnion Power;
    Audio_ByteUnion Audio;
    Wireless_ByteUnion Wireless;
    Memory_ByteUnion Memory;
}Device_Err_Typedef;

typedef struct {
    uint8_t fault_code;   // 故障码数字部分（如1, 2, 11, 12等）
    uint8_t byte_offset;  // byte位偏移
    uint8_t bit_mask;     // 位掩码
    uint8_t module;       // 模块索引
} Fault_Info;

#define t_1()				    (GPIOC->BSRR = GPIO_PIN_10 )
#define t_0()				    (GPIOC->BSRR = GPIO_PIN_10 << 16)

#define m_1()					(GPIOC->BSRR = GPIO_PIN_11 )
#define m_0()					(GPIOC->BSRR = GPIO_PIN_11 << 16)


extern volatile Device_Err_Typedef g_Err;
extern volatile Systeam_Mode_EnumDef eSystemMode;
void System_Progress(void);
uint8_t Err_Code_Show_handle(void);

#ifdef __cplusplus
}
#endif
#endif  // APP_SYSTEM_H
/**************************End of file********************************/

