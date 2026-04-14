/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_power.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_POWER_H
#define APP_POWER_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_system.h"
#include "app_audio.h"

#define POWER_OS_DELAY  30

#define POWER_UP_AUTOMACTICALLY     1
#define POWER_HARD_VER              3


#define BAT_100perVolt  405
#define BAT_80perVolt   390
#define BAT_60perVolt   380
#define BAT_40perVolt   370
#define BAT_20perVolt   310

typedef enum
{
	E_NONE_BAT_MODE,
    E_LOW_POWER_MODE,
    E_CHARGING_MODE,
    E_BAT_FULL_MODE,

}Power_Led_EnumDef;

typedef struct {
    uint16_t BAT;    	
    uint16_t DCIN;   	
    uint16_t Vol_5V0;  
    uint16_t Vol_3V3; 
    Power_Led_EnumDef State;
    uint8_t BatValue;
} Power_Voltage_TypeDef;

typedef union {
    uint16_t byte;  
    struct {
        uint16_t audio : 1;
        uint16_t excom : 1;
        uint16_t feedback : 1;
        uint16_t memory : 1;
        uint16_t wireless : 1;
        uint16_t power : 1;
        uint16_t bit6 : 1;
        uint16_t bit7 : 1;
    } bits;
} PowerDown_State_ByteUnion;

typedef enum
{
    DEV_POWER_ON = 1,
    DEV_POWER_OFF = 2,

}Power_State_EnumDef;

typedef struct {
    Power_State_EnumDef state;
    PowerDown_State_ByteUnion flag;
} PowerDown_State_StructDef;


void App_Hard_Ver_Read(void);
void App_Power_Manager(void);
void App_Power_CheckPowerBtn(void);
void App_Power_Self_Check(void);
void App_Power_ShutDown(void);

#ifdef __cplusplus
}
#endif
#endif  // APP_POWER_H
/**************************End of file********************************/

