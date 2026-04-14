/***********************************************************************************
* @file     : systask.c
* @brief    : Bare-metal system task scheduler implementation.
* @details  : Dispatches display and flash tasks with independent periods on
*             top of a 1 ms cooperative base tick while maintaining a shared
*             runtime snapshot.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "systask.h"

#include <stddef.h>

#include "display.h"
#include "flash.h"
#include "drv_delay.h"
#include "log.h"

#define SYSTASK_LOG_TAG                   "SysTask"

typedef struct stSystemTaskRuntimeState {
    uint32_t startTick;
    stSystemTaskSnapshot snapshot;
} stSystemTaskRuntimeState;

static bool gSystemTaskSchedulerInitialized = false;
static uint32_t gSystemTaskBaseTick = 0U;
static stSystemTaskRuntimeState gSystemTaskRuntimeState;

static stSystemTaskState gSystemTaskStates[E_SYSTEM_TASK_COUNT] = {
    { false, 0U, 0U },
    { false, 0U, 0U },
};

static bool systemDisplayModuleInit(void);
static void systemDisplayModuleProcess(uint32_t nowTick);
static bool systemFlashModuleInit(void);
static void systemFlashModuleProcess(uint32_t nowTick);
static void systemTaskSnapshotReset(void);
static void systemTaskSnapshotUpdate(uint32_t nowTick);
static void systemTaskSnapshotSetDisplayReady(bool isReady);
static void systemTaskSnapshotSetDisplayValue(uint16_t value);
static void systemTaskSnapshotSetFlashInfo(bool isReady, uint8_t manufacturerId, uint8_t memoryType, uint8_t capacityId);

static const stSystemTaskConfig gSystemTaskConfigs[] = {
    { "display", SYSTEM_TASK_DISPLAY_INTERVAL_MS, SYSTEM_TASK_DISPLAY_INIT_RETRY_MS, systemDisplayModuleInit, systemDisplayModuleProcess },
    { "flash", SYSTEM_TASK_FLASH_INTERVAL_MS, SYSTEM_TASK_FLASH_INIT_RETRY_MS, systemFlashModuleInit, systemFlashModuleProcess },
};

bool systemTaskSchedulerInit(void)
{
    uint32_t lIndex;

    displayManagerReset();
    flashManagerReset();
    systemTaskSnapshotReset();

    for (lIndex = 0U; lIndex < (uint32_t)(sizeof(gSystemTaskStates) / sizeof(gSystemTaskStates[0])); lIndex++) {
        gSystemTaskStates[lIndex].isReady = false;
        gSystemTaskStates[lIndex].lastRunTick = 0U;
        gSystemTaskStates[lIndex].lastInitAttemptTick = 0U;
    }

    gSystemTaskBaseTick = Drv_GetTick();
    gSystemTaskRuntimeState.startTick = gSystemTaskBaseTick;
    gSystemTaskSchedulerInitialized = true;
    LOG_I(SYSTASK_LOG_TAG, "bare-metal scheduler ready, base=%lu ms", (unsigned long)SYSTEM_TASK_BASE_PERIOD_MS);
    return true;
}

void systemTaskGetSnapshot(stSystemTaskSnapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    *snapshot = gSystemTaskRuntimeState.snapshot;
}

void systemTaskSchedulerProcess(void)
{
    uint32_t lNowTick;
    uint32_t lIndex;

    if (!gSystemTaskSchedulerInitialized) {
        return;
    }

    lNowTick = Drv_GetTick();
    if ((lNowTick - gSystemTaskBaseTick) < SYSTEM_TASK_BASE_PERIOD_MS) {
        return;
    }

    gSystemTaskBaseTick = lNowTick;
    systemTaskSnapshotUpdate(lNowTick);

    for (lIndex = 0U; lIndex < (uint32_t)(sizeof(gSystemTaskConfigs) / sizeof(gSystemTaskConfigs[0])); lIndex++) {
        stSystemTaskState *lState = &gSystemTaskStates[lIndex];
        const stSystemTaskConfig *lConfig = &gSystemTaskConfigs[lIndex];

        if (!lState->isReady) {
            if ((lNowTick - lState->lastInitAttemptTick) < lConfig->initRetryMs) {
                continue;
            }

            lState->lastInitAttemptTick = lNowTick;
            lState->isReady = lConfig->init();
            if (!lState->isReady) {
                continue;
            }

            lState->lastRunTick = lNowTick - lConfig->intervalMs;
            LOG_I(SYSTASK_LOG_TAG,
                  "%s task ready, interval=%lu ms",
                  lConfig->name,
                  (unsigned long)lConfig->intervalMs);
        }

        if ((lNowTick - lState->lastRunTick) < lConfig->intervalMs) {
            continue;
        }

        lState->lastRunTick = lNowTick;
        lConfig->process(lNowTick);
    }
}

static void systemTaskSnapshotReset(void)
{
    gSystemTaskRuntimeState.startTick = 0U;
    gSystemTaskRuntimeState.snapshot.uptimeMs = 0U;
    gSystemTaskRuntimeState.snapshot.heartbeat = 0U;
    gSystemTaskRuntimeState.snapshot.displayValue = 0U;
    gSystemTaskRuntimeState.snapshot.isDisplayReady = false;
    gSystemTaskRuntimeState.snapshot.isFlashReady = false;
    gSystemTaskRuntimeState.snapshot.flashManufacturerId = 0U;
    gSystemTaskRuntimeState.snapshot.flashMemoryType = 0U;
    gSystemTaskRuntimeState.snapshot.flashCapacityId = 0U;
}

static void systemTaskSnapshotUpdate(uint32_t nowTick)
{
    gSystemTaskRuntimeState.snapshot.uptimeMs = nowTick - gSystemTaskRuntimeState.startTick;
    gSystemTaskRuntimeState.snapshot.heartbeat++;

    if (!gSystemTaskRuntimeState.snapshot.isDisplayReady) {
        gSystemTaskRuntimeState.snapshot.displayValue = (uint16_t)((gSystemTaskRuntimeState.snapshot.uptimeMs / 100U) % 1000U);
    }
}

static void systemTaskSnapshotSetDisplayReady(bool isReady)
{
    gSystemTaskRuntimeState.snapshot.isDisplayReady = isReady;
}

static void systemTaskSnapshotSetDisplayValue(uint16_t value)
{
    gSystemTaskRuntimeState.snapshot.displayValue = value;
}

static void systemTaskSnapshotSetFlashInfo(bool isReady, uint8_t manufacturerId, uint8_t memoryType, uint8_t capacityId)
{
    gSystemTaskRuntimeState.snapshot.isFlashReady = isReady;
    gSystemTaskRuntimeState.snapshot.flashManufacturerId = manufacturerId;
    gSystemTaskRuntimeState.snapshot.flashMemoryType = memoryType;
    gSystemTaskRuntimeState.snapshot.flashCapacityId = capacityId;
}

static bool systemDisplayModuleInit(void)
{
    bool lIsReady = displayManagerInit();

    systemTaskSnapshotSetDisplayReady(lIsReady);
    return lIsReady;
}

static void systemDisplayModuleProcess(uint32_t nowTick)
{
    stSystemTaskSnapshot lSnapshot;
    stDisplayManagerInput lInput;
    uint16_t lDisplayValue;

    (void)nowTick;

    systemTaskGetSnapshot(&lSnapshot);

    lInput.heartbeat = lSnapshot.heartbeat;
    lInput.displayValue = lSnapshot.displayValue;
    lInput.isFlashReady = lSnapshot.isFlashReady;

    if (!displayManagerProcess(&lInput, &lDisplayValue)) {
        gSystemTaskStates[E_SYSTEM_TASK_DISPLAY].isReady = false;
        displayManagerReset();
        systemTaskSnapshotSetDisplayReady(false);
        return;
    }

    systemTaskSnapshotSetDisplayReady(true);
    systemTaskSnapshotSetDisplayValue(lDisplayValue);
}

static bool systemFlashModuleInit(void)
{
    stFlashManagerInfo lInfo;
    bool lIsReady = flashManagerInit(&lInfo);

    systemTaskSnapshotSetFlashInfo(lInfo.isReady,
                                   lInfo.manufacturerId,
                                   lInfo.memoryType,
                                   lInfo.capacityId);
    return lIsReady;
}

static void systemFlashModuleProcess(uint32_t nowTick)
{
    stFlashManagerInfo lInfo;

    flashManagerProcess(nowTick, &lInfo);
    systemTaskSnapshotSetFlashInfo(lInfo.isReady,
                                   lInfo.manufacturerId,
                                   lInfo.memoryType,
                                   lInfo.capacityId);
}

/**************************End of file********************************/
