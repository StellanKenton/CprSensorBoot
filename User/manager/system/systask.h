/************************************************************************************
* @file     : systask.h
* @brief    : Bare-metal system task scheduler declarations.
* @details  : Provides the cooperative task dispatcher used by the bootloader
*             main loop.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_SYSTASK_H
#define CPRSENSORBOOT_SYSTASK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_TASK_BASE_PERIOD_MS        1U
#define SYSTEM_TASK_DISPLAY_INTERVAL_MS   20U
#define SYSTEM_TASK_DISPLAY_INIT_RETRY_MS 200U

#define SYSTEM_TASK_FLASH_INTERVAL_MS     50U
#define SYSTEM_TASK_FLASH_INIT_RETRY_MS   200U

typedef bool (*systemTaskInitFunc)(void);
typedef void (*systemTaskProcessFunc)(uint32_t nowTick);

typedef struct stSystemTaskConfig {
    const char *name;
    uint32_t intervalMs;
    uint32_t initRetryMs;
    systemTaskInitFunc init;
    systemTaskProcessFunc process;
} stSystemTaskConfig;

typedef struct stSystemTaskState {
    bool isReady;
    uint32_t lastRunTick;
    uint32_t lastInitAttemptTick;
} stSystemTaskState;

typedef enum eSystemTaskIndex {
    E_SYSTEM_TASK_DISPLAY = 0,
    E_SYSTEM_TASK_FLASH,
    E_SYSTEM_TASK_COUNT
} eSystemTaskIndex;

typedef struct stSystemTaskSnapshot {
	uint32_t uptimeMs;
	uint32_t heartbeat;
	uint16_t displayValue;
	bool isDisplayReady;
	bool isFlashReady;
	uint8_t flashManufacturerId;
	uint8_t flashMemoryType;
	uint8_t flashCapacityId;
} stSystemTaskSnapshot;

bool systemTaskSchedulerInit(void);
void systemTaskSchedulerProcess(void);
void systemTaskGetSnapshot(stSystemTaskSnapshot *snapshot);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_SYSTASK_H
/**************************End of file********************************/
