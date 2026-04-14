/***********************************************************************************
* @file     : sysmgr.c
* @brief    : System manager implementation.
* @details  : Bridges the main loop, cooperative scheduler, watchdog, and RTT
*             console entry points.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "sysmgr.h"

#include "adc.h"
#include "dma.h"
#include "gpio.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"

#include "cm_backtrace.h"
#include "console.h"
#include "drv_delay.h"
#include "drv_wdg.h"
#include "log.h"
#include "systask.h"
#include "system_debug.h"
#include "update.h"

static stSystemManagerState gSystemManagerState;

static bool systemConsoleEnsureReady(void);
static void systemLogPump(void);
static void systemWatchdogProcess(void);

bool systemManagerInit(void)
{
    gSystemManagerState.isInitialized = false;
    gSystemManagerState.isConsoleReady = false;
    gSystemManagerState.watchdogTick = 0U;

    (void)systemConsoleEnsureReady();

    Drv_WatchDogResartCheck();
    cm_backtrace_init(systemGetFirmwareName(), systemGetHardwareVersion(), systemGetFirmwareVersion());

    LOG_I(SYSTEM_MANAGER_LOG_TAG, "&&&&&&&&&&&&&&&&& BOOT LOADER &&&&&&&&&&&&&&&&&");
    LOG_I(SYSTEM_MANAGER_LOG_TAG,
          "Firmware: %s, Version: %s, Hardware: %s",
          systemGetFirmwareName(),
          systemGetFirmwareVersion(),
          systemGetHardwareVersion());
    systemLogPump();

    if (Drv_WatchDog_Init(0U) == 0U) {
        gSystemManagerState.watchdogTick = Drv_GetTick();
        LOG_I(SYSTEM_MANAGER_LOG_TAG,
              "IWDG started, feed interval=%lu ms",
              (unsigned long)SYSTEM_WDG_FEED_INTERVAL_MS);
    } else {
        LOG_W(SYSTEM_MANAGER_LOG_TAG, "IWDG start failed");
    }

    (void)systemTaskSchedulerInit();
    (void)updateInit();
    systemLogPump();

    gSystemManagerState.isInitialized = true;
    (void)systemSetMode(E_SYSTEM_UPDATE_MODE);
    systemLogPump();
    return true;
}

void systemInitModeRun(void)
{
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_SPI1_Init();
    MX_TIM7_Init();
    MX_RTC_Init();
    //MX_IWDG_Init();
    MX_UART4_Init();
    (void)systemManagerInit();
    gSystemManagerState.isInitialized = true;
    systemSetMode(E_SYSTEM_CHECK_MODE);
}

void systemCheckModeRun(void)
{
    systemSetMode(E_SYSTEM_UPDATE_MODE);
}

void systemUpdateModeRun(void)
{
    const stUpdateStatus *lUpdateStatus = updateGetStatus();

    updateProcess(Drv_GetTick());

    lUpdateStatus = updateGetStatus();
    if ((lUpdateStatus != NULL) && lUpdateStatus->isUpdateRequested) {
        return;
    }

    systemTaskSchedulerProcess();
}


void systemFunctionStateMachine(void)
{
    uint32_t lNowTick = Drv_GetTick();

    if ((lNowTick - gSystemManagerState.watchdogTick) < SYSTEM_FSM_INTERVAL_MS) {
        return;
    }

    switch (systemGetMode()) {
        case E_SYSTEM_INIT_MODE:
            systemInitModeRun();
            break;
        case E_SYSTEM_CHECK_MODE:
            systemCheckModeRun();
            break;
        case E_SYSTEM_UPDATE_MODE:
            systemUpdateModeRun();
            break;
        default:
            break;
    }
}

void systemManagerProcess(void)
{  
    systemFunctionStateMachine();       // run the current mode's main function
    if (systemConsoleEnsureReady()) {   // console process
        consoleProcess();
    } 
    systemWatchdogProcess();            // feed the dog
}

static bool systemConsoleEnsureReady(void)
{
    if (gSystemManagerState.isConsoleReady) {
        return true;
    }

    if (!consoleInit()) {
        LOG_E(SYSTEM_MANAGER_LOG_TAG, "console init failed");
        return false;
    }

    if (!systemDebugConsoleRegister()) {
        LOG_E(SYSTEM_MANAGER_LOG_TAG, "register system debug failed");
        return false;
    }

    gSystemManagerState.isConsoleReady = true;
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "RTT console ready");
    return true;
}

static void systemLogPump(void)
{
    if (!gSystemManagerState.isConsoleReady) {
        return;
    }

    logProcessOutput();
}

static void systemWatchdogProcess(void)
{
    uint32_t lNowTick = Drv_GetTick();

    if ((lNowTick - gSystemManagerState.watchdogTick) < SYSTEM_WDG_FEED_INTERVAL_MS) {
        return;
    }

    Drv_WatchDogFeed();
    gSystemManagerState.watchdogTick = lNowTick;
}
/**************************End of file********************************/
