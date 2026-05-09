/***********************************************************************************
* @file     : update_mgr.c
* @brief    : Update manager adapter implementation.
* @details  : Bridges project-specific watchdog and jump handling to the reusable
*             rep update service.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update_mgr.h"

#include <string.h>

#include "drv_wdg.h"
#include "log.h"
#include "main.h"
#include "drvmcuflash.h"

#include "../../../rep/service/update/update_port.h"

#define UPDATE_MANAGER_LOG_TAG             "UpdateMgr"
#define UPDATE_APP_VECTOR_STACK_MASK       0x2FFE0000UL
#define UPDATE_APP_VECTOR_STACK_BASE       0x20000000UL
#define UPDATE_APP_VECTOR_ENTRY_MASK       0xFFFFFFFEUL
#define UPDATE_NVIC_REGISTER_COUNT         8U

static bool updateManagerCanBootNormally(const stUpdateBootRecord *record);
static bool updateManagerGetExecutableRegion(uint8_t regionId, stUpdateRegionCfg *cfg);
static bool updateManagerReadAppVector(const stUpdateRegionCfg *cfg, uint32_t vector[2]);
static bool updateManagerJumpToRegionInternal(uint8_t regionId);
static void updateManagerPrepareAppJump(uint32_t vectorBase);
static __NO_RETURN __ASM void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler);

void updateManagerReset(void)
{
    updateReset();
}

bool updateManagerInit(void)
{
    if (!updateInit()) {
        return false;
    }

#if (UPDATE_MANAGER_FORCE_RUN_APP_BOOT_RECORD == 1)
    if (!updateManagerSetBootRecordRunApp()) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "force run_app boot record failed");
        return false;
    }
#endif

    return true;
}

void updateManagerProcess(uint32_t nowTickMs)
{
    const stUpdateStatus *lStatus;

    updateProcess(nowTickMs);

    lStatus = updateGetStatus();
    if ((lStatus != NULL) && (lStatus->state == E_UPDATE_STATE_JUMP_TARGET)) {
        (void)updateJumpToTargetIfValid();
    }
}

const stUpdateStatus *updateManagerGetStatus(void)
{
    return updateGetStatus();
}

bool updateManagerGetBootRecord(stUpdateBootRecord *record)
{
    return updateReadBootRecord(record);
}

bool updateManagerHasNormalAppBootFlag(void)
{
    stUpdateBootRecord lRecord;

    if (!updateReadBootRecord(&lRecord)) {
        return false;
    }

    return updateManagerCanBootNormally(&lRecord);
}

bool updateManagerJumpToAppIfValid(void)
{
    return updateJumpToTargetIfValid();
}

bool updateManagerSetBootRecordRunApp(void)
{
    stUpdateBootRecord lRecord;

    if (!updateReadBootRecord(&lRecord)) {
        (void)memset(&lRecord, 0, sizeof(lRecord));
        lRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    }

    lRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    lRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_RUN_APP;
    lRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    lRecord.targetRegion = (uint32_t)E_UPDATE_REGION_RUN_APP;

    if (!updateWriteBootRecord(&lRecord)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "store run_app boot record failed");
        return false;
    }

    LOG_I(UPDATE_MANAGER_LOG_TAG, "boot record forced to run_app");
    return true;
}

static bool updateManagerCanBootNormally(const stUpdateBootRecord *record)
{
    if (record == NULL) {
        return false;
    }

    if (record->magic != UPDATE_BOOT_RECORD_MAGIC) {
        return false;
    }

    return (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_IDLE) ||
           (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_RUN_APP) ||
           (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_FAILED);
}

static bool updateManagerGetExecutableRegion(uint8_t regionId, stUpdateRegionCfg *cfg)
{
    if ((cfg == NULL) || !updatePortGetRegionMap(regionId, cfg)) {
        return false;
    }

    return cfg->isExecutable && (cfg->storageId == E_UPDATE_STORAGE_INTERNAL_FLASH) && (cfg->size >= 8U);
}

static bool updateManagerReadAppVector(const stUpdateRegionCfg *cfg, uint32_t vector[2])
{
    if ((cfg == NULL) || (vector == NULL)) {
        return false;
    }

    return drvMcuFlashRead(cfg->startAddress, (uint8_t *)vector, sizeof(uint32_t) * 2U);
}

static bool updateManagerJumpToRegionInternal(uint8_t regionId)
{
    stUpdateRegionCfg lRegionCfg;
    uint32_t lVector[2] = {0U, 0U};
    uint32_t lResetHandler;

    if (!updateManagerGetExecutableRegion(regionId, &lRegionCfg)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "jump region invalid, region=%u", (unsigned int)regionId);
        return false;
    }

    if (!updateManagerReadAppVector(&lRegionCfg, lVector)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "read app vector failed, base=0x%08lX", (unsigned long)lRegionCfg.startAddress);
        return false;
    }

    if ((lVector[0] & UPDATE_APP_VECTOR_STACK_MASK) != UPDATE_APP_VECTOR_STACK_BASE) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "invalid app stack pointer: 0x%08lX", (unsigned long)lVector[0]);
        return false;
    }

    if ((lVector[1] & 0x1U) == 0U) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "invalid app reset vector: 0x%08lX", (unsigned long)lVector[1]);
        return false;
    }

    lResetHandler = lVector[1] & UPDATE_APP_VECTOR_ENTRY_MASK;
    if ((lResetHandler < lRegionCfg.startAddress) ||
        (lResetHandler >= (lRegionCfg.startAddress + lRegionCfg.size))) {
        LOG_E(UPDATE_MANAGER_LOG_TAG,
              "app reset vector out of range: 0x%08lX",
              (unsigned long)lResetHandler);
        return false;
    }

    LOG_I(UPDATE_MANAGER_LOG_TAG,
          "jump to region %u: sp=0x%08lX reset=0x%08lX",
          (unsigned int)regionId,
          (unsigned long)lVector[0],
          (unsigned long)lVector[1]);
    updateManagerPrepareAppJump(lRegionCfg.startAddress);
    updateManagerLaunchApp(lVector[0], lVector[1]);
    return false;
}

static void updateManagerPrepareAppJump(uint32_t vectorBase)
{
    uint32_t lIndex;

    __disable_irq();
    HAL_DeInit();
    HAL_RCC_DeInit();
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (lIndex = 0U; lIndex < UPDATE_NVIC_REGISTER_COUNT; lIndex++) {
        NVIC->ICER[lIndex] = 0xFFFFFFFFUL;
        NVIC->ICPR[lIndex] = 0xFFFFFFFFUL;
    }

    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk);
    SCB->CFSR = 0xFFFFFFFFUL;
    SCB->HFSR = 0xFFFFFFFFUL;
    SCB->DFSR = 0xFFFFFFFFUL;
    SCB->AFSR = 0xFFFFFFFFUL;

    __set_CONTROL(0U);
    __set_PSP(0U);
    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);
    SCB->VTOR = vectorBase;

    __DSB();
    __ISB();
}

static __NO_RETURN __ASM void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler)
{
    MSR MSP, r0
    MOVS r0, #0
    MSR PRIMASK, r0
    DSB
    ISB
    BX  r1
}

void updatePortFeedWatchdog(void)
{
    Drv_WatchDogFeed();
}

bool updatePortJumpToRegion(uint8_t regionId)
{
    return updateManagerJumpToRegionInternal(regionId);
}

/**************************End of file********************************/
