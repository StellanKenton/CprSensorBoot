/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_system.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_system.h"
#include "log.h"
#include "cm_backtrace.h"
#include "iwdg.h"
#include "app_memory.h"
#include "drv_wdg.h"
#include "app_bootloader.h"
#include "drv_blemodule.h"
#include "app_wireless.h"


static System_Mgr_t s_SystemMgr = {E_SYSTEM_STANDBY_MODE, 0};



void System_ChangeMode(System_Mode_EnumDef newMode)
{
    if(newMode != s_SystemMgr.eMode && newMode < E_SYSTEM_MODE_MAX){
        s_SystemMgr.eMode = newMode;
        LOG_I("System mode changed to %d", newMode);
    }
}

void System_Init(void)
{
    Log_Init();
    Drv_WatchDogResartCheck();
    cm_backtrace_init(FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_VERSION);
    LOG_I("&&&&&&&&&&&&&&&&& BOOT LOADER &&&&&&&&&&&&&&&&&");
    LOG_I("System initialized.");
    LOG_I("Firmware: %s, Version: %s, Hardware: %s", FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_VERSION);    
    App_Memory_init();     
    WireLess_Init();     
}

void SystemManager(void)
{
    switch(s_SystemMgr.eMode)
    {
        case E_SYSTEM_STANDBY_MODE:
            // Handle standby mode
            App_BootLoader_JumpCheck();
            if (App_Bootloader_GetNeedUpdate()) {
                System_ChangeMode(E_SYSTEM_UPDATE_MODE);
            }
            break;
        case E_SYSTEM_NORMAL_MODE:
            // Handle normal mode
            // Do nothing for now
            break;
        case E_SYSTEM_UPDATE_MODE:
            // Handle update mode
            App_Bootloader_Manager();
            WireLess_Process();
            Log_Process(1);
            break;
        case E_SYSTEM_MODE_MAX:
        default:
            // Handle unexpected mode
            break;
    }
    
}

/**
* @brief SystemProcess
* @retval None
**/
void SystemProcess(void)
{
    SystemManager();
    //Drv_WatchDogFeed();
}
/**************************End of file********************************/


