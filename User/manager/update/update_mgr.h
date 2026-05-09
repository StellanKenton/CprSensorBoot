/***********************************************************************************
* @file     : update_mgr.h
* @brief    : Update manager adapter declarations.
* @details  : Adapts the project system manager to the reusable rep update service.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_UPDATE_MGR_H
#define CPRSENSORBOOT_UPDATE_MGR_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../rep/service/update/update.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UPDATE_MANAGER_FORCE_RUN_APP_BOOT_RECORD
#define UPDATE_MANAGER_FORCE_RUN_APP_BOOT_RECORD   0
#endif

void updateManagerReset(void);
bool updateManagerInit(void);
void updateManagerProcess(uint32_t nowTickMs);
const stUpdateStatus *updateManagerGetStatus(void);
bool updateManagerGetBootRecord(stUpdateBootRecord *record);
bool updateManagerHasNormalAppBootFlag(void);
bool updateManagerJumpToAppIfValid(void);
bool updateManagerSetBootRecordRunApp(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_UPDATE_MGR_H
/**************************End of file********************************/
