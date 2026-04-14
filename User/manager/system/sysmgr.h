/************************************************************************************
* @file     : sysmgr.h
* @brief    : System manager declarations.
* @details  : Owns bootloader system startup, mode switching, watchdog feeding,
*             and console integration.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_SYSMGR_H
#define CPRSENSORBOOT_SYSMGR_H

#include <stdbool.h>
#include <stdint.h>

#include "system.h"

#ifdef __cplusplus
extern "C" {
#endif
#define SYSTEM_MANAGER_LOG_TAG              "SysMgr"
#define SYSTEM_WDG_FEED_INTERVAL_MS         1000U
#define SYSTEM_FSM_INTERVAL_MS              10U

typedef struct stSystemManagerState {
    bool isInitialized;
    bool isConsoleReady;
    uint32_t watchdogTick;
} stSystemManagerState;

bool systemManagerInit(void);
void systemManagerProcess(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_SYSMGR_H
/**************************End of file********************************/
