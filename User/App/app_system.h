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
#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif


#define FIRMWARE_NAME           "CprSensorBoot"

// Version string helpers to keep format v1.x.x without manual string edits
#define STR_HELPER(x)           #x
#define STR(x)                  STR_HELPER(x)

// Update these numbers only
#define FW_VER_MAJOR            1
#define FW_VER_MINOR            0
#define FW_VER_PATCH            1

#define HW_VER_MAJOR            1
#define HW_VER_MINOR            0
#define HW_VER_PATCH            0

#define FIRMWARE_VERSION        "v" STR(FW_VER_MAJOR) "." STR(FW_VER_MINOR) "." STR(FW_VER_PATCH)
#define HARDWARE_VERSION        "v" STR(HW_VER_MAJOR) "." STR(HW_VER_MINOR) "." STR(HW_VER_PATCH)


typedef enum
{
    E_SYSTEM_STANDBY_MODE = 0,
    E_SYSTEM_NORMAL_MODE,
    E_SYSTEM_UPDATE_MODE,
    E_SYSTEM_MODE_MAX,
} System_Mode_EnumDef;

typedef struct 
{
    System_Mode_EnumDef eMode;
    uint32_t sysTick;
} System_Mgr_t;

void System_Init(void);
void SystemProcess(void);
#ifdef __cplusplus
}
#endif
#endif  // APP_SYSTEM_H
/**************************End of file********************************/

