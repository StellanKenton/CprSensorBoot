/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_system.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_system.h"
#include "log.h"
#include "console.h"
#include "cm_backtrace.h"
#include "drv_delay.h"
#include "drv_wdg.h"
#include "drvspi_debug.h"
#include "gd25qxxx_debug.h"
#include "pca9535_debug.h"
#include "pca9535_port.h"
#include "tm1651_debug.h"
#include "tm1651_port.h"

#define APP_SYSTEM_LOG_TAG    "AppSystem"
#define APP_SYSTEM_WDG_FEED_INTERVAL_MS 1000U

static bool systemConsoleEnsureReady(void);
static const char *systemGetModeString(System_Mode_EnumDef mode);
static void systemBoardInit(void);
static void systemWatchdogProcess(void);
static eConsoleCommandResult systemConsoleVersionHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemConsoleStatusHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemConsoleRebootHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gSystemVersionConsoleCommand = {
    .commandName = "ver",
    .helpText = "ver - show firmware and hardware version",
    .ownerTag = "system",
    .handler = systemConsoleVersionHandler,
};

static const stConsoleCommand gSystemStatusConsoleCommand = {
    .commandName = "sys",
    .helpText = "sys - show boot system status",
    .ownerTag = "system",
    .handler = systemConsoleStatusHandler,
};

static const stConsoleCommand gSystemRebootConsoleCommand = {
    .commandName = "reboot",
    .helpText = "reboot - reboot device immediately",
    .ownerTag = "system",
    .handler = systemConsoleRebootHandler,
};

static System_Mgr_t s_SystemMgr = {E_SYSTEM_STANDBY_MODE, 0};



void System_ChangeMode(System_Mode_EnumDef newMode)
{
    if(newMode != s_SystemMgr.eMode && newMode < E_SYSTEM_MODE_MAX){
        s_SystemMgr.eMode = newMode;
        LOG_I(APP_SYSTEM_LOG_TAG, "System mode changed to %s(%d)", systemGetModeString(newMode), (int)newMode);
    }
}

void System_Init(void)
{
    Drv_WatchDogResartCheck();
    cm_backtrace_init(FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_VERSION);
    LOG_I(APP_SYSTEM_LOG_TAG, "&&&&&&&&&&&&&&&&& BOOT LOADER &&&&&&&&&&&&&&&&&");
    LOG_I(APP_SYSTEM_LOG_TAG, "System initialized.");
    LOG_I(APP_SYSTEM_LOG_TAG, "Firmware: %s, Version: %s, Hardware: %s", FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_VERSION);
    systemBoardInit();
    if (Drv_WatchDog_Init(0U) == 0U) {
        s_SystemMgr.sysTick = Drv_GetTick();
        LOG_I(APP_SYSTEM_LOG_TAG, "IWDG started. Feed interval: %lu ms", (unsigned long)APP_SYSTEM_WDG_FEED_INTERVAL_MS);
    }
    System_ChangeMode(E_SYSTEM_UPDATE_MODE);
}

void SystemManager(void)
{
    switch(s_SystemMgr.eMode)
    {
        case E_SYSTEM_STANDBY_MODE:
            // Handle standby mode
            break;
        case E_SYSTEM_NORMAL_MODE:
            // Handle normal mode
            // Do nothing for now
            break;
        case E_SYSTEM_UPDATE_MODE:
            if (systemConsoleEnsureReady()) {
                consoleProcess();
            }
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
    systemWatchdogProcess();
}

static void systemBoardInit(void)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortInit();
    if (lStatus != DRV_STATUS_OK) {
        LOG_E(APP_SYSTEM_LOG_TAG, "TM1651 board init failed, status=%d", (int)lStatus);
        return;
    }

    lStatus = tm1651PortClearDisplay();
    if (lStatus != DRV_STATUS_OK) {
        LOG_E(APP_SYSTEM_LOG_TAG, "TM1651 default state failed, status=%d", (int)lStatus);
        return;
    }

    lStatus = pca9535PortInit();
    if (lStatus != DRV_STATUS_OK) {
        LOG_E(APP_SYSTEM_LOG_TAG, "PCA9535 board init failed, status=%d", (int)lStatus);
        return;
    }

    lStatus = pca9535PortLedOff();
    if (lStatus != DRV_STATUS_OK) {
        LOG_E(APP_SYSTEM_LOG_TAG, "PCA9535 default state failed, status=%d", (int)lStatus);
        return;
    }

    LOG_I(APP_SYSTEM_LOG_TAG, "TM1651 and PCA9535 board mapping initialized");
}

static void systemWatchdogProcess(void)
{
    uint32_t lNowTick = Drv_GetTick();

    if ((lNowTick - s_SystemMgr.sysTick) < APP_SYSTEM_WDG_FEED_INTERVAL_MS) {
        return;
    }

    Drv_WatchDogFeed();
    s_SystemMgr.sysTick = lNowTick;
}

static bool systemConsoleEnsureReady(void)
{
    static bool gConsoleReady = false;

    if (gConsoleReady) {
        return true;
    }

    if (!consoleInit()) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Console init failed");
        return false;
    }

    if (!consoleRegisterCommand(&gSystemVersionConsoleCommand)) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Register version command failed");
        return false;
    }

    if (!consoleRegisterCommand(&gSystemStatusConsoleCommand)) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Register status command failed");
        return false;
    }

    if (!consoleRegisterCommand(&gSystemRebootConsoleCommand)) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Register reboot command failed");
        return false;
    }

    if (!drvSpiDebugConsoleRegister()) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Register spi command failed");
        return false;
    }

    if (!gd25qxxxDebugConsoleRegister()) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Register gd25qxxx command failed");
        return false;
    }

    if (!pca9535DebugConsoleRegister()) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Register pca9535 command failed");
        return false;
    }

    if (!tm1651DebugConsoleRegister()) {
        LOG_E(APP_SYSTEM_LOG_TAG, "Register tm1651 command failed");
        return false;
    }

    gConsoleReady = true;
    LOG_I(APP_SYSTEM_LOG_TAG, "Console initialized on RTT");
    return true;
}

static const char *systemGetModeString(System_Mode_EnumDef mode)
{
    switch (mode) {
        case E_SYSTEM_STANDBY_MODE:
            return "STANDBY_MODE";
        case E_SYSTEM_NORMAL_MODE:
            return "NORMAL_MODE";
        case E_SYSTEM_UPDATE_MODE:
            return "UPDATE_MODE";
        default:
            return "UNKNOWN_MODE";
    }
}

static eConsoleCommandResult systemConsoleVersionHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport,
        "Firmware: %s\nVersion: %s\nHardware: %s\nOK",
        FIRMWARE_NAME,
        FIRMWARE_VERSION,
        HARDWARE_VERSION) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemConsoleStatusHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport,
        "Mode: %s\nOK",
        systemGetModeString(s_SystemMgr.eMode)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemConsoleRebootHandler(uint32_t transport, int argc, char *argv[])
{
    uint32_t lFlushCount = 0U;

    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport, "Rebooting device...") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    for (lFlushCount = 0U; lFlushCount < 4U; lFlushCount++) {
        logProcessOutput();
    }

    NVIC_SystemReset();
    return CONSOLE_COMMAND_RESULT_OK;
}
/**************************End of file********************************/


