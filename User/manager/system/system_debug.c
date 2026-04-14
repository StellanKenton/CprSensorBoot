/***********************************************************************************
* @file     : system_debug.c
* @brief    : System debug and console command implementation.
* @details  : Hosts system console commands and optional module debug command
*             registration for the bootloader system layer.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system_debug.h"

#include <stdint.h>

#include "console.h"
#include "drvspi_debug.h"
#include "gd25qxxx_debug.h"
#include "log.h"
#include "main.h"
#include "pca9535_debug.h"
#include "systask.h"
#include "system.h"
#include "tm1651_debug.h"

static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleRebootHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gSystemVersionConsoleCommand = {
    .commandName = "ver",
    .helpText = "ver - show firmware and hardware version",
    .ownerTag = "system",
    .handler = systemDebugConsoleVersionHandler,
};

static const stConsoleCommand gSystemStatusConsoleCommand = {
    .commandName = "sys",
    .helpText = "sys - show bootloader scheduler status",
    .ownerTag = "system",
    .handler = systemDebugConsoleStatusHandler,
};

static const stConsoleCommand gSystemRebootConsoleCommand = {
    .commandName = "reboot",
    .helpText = "reboot - reboot device immediately",
    .ownerTag = "system",
    .handler = systemDebugConsoleRebootHandler,
};

static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport,
                     "Firmware: %s\nVersion: %s\nHardware: %s\nOK",
                     systemGetFirmwareName(),
                     systemGetFirmwareVersion(),
                     systemGetHardwareVersion()) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[])
{
    stSystemTaskSnapshot lSnapshot;

    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    systemTaskGetSnapshot(&lSnapshot);
    if (consoleReply(transport,
                     "Mode: %s\nUptime: %lu ms\nHeartbeat: %lu\nDisplay: %s value=%u\nFlash: %s jedec=%02X %02X %02X\nOK",
                     systemGetModeString(systemGetMode()),
                     (unsigned long)lSnapshot.uptimeMs,
                     (unsigned long)lSnapshot.heartbeat,
                     lSnapshot.isDisplayReady ? "ready" : "down",
                     (unsigned int)lSnapshot.displayValue,
                     lSnapshot.isFlashReady ? "ready" : "down",
                     lSnapshot.flashManufacturerId,
                     lSnapshot.flashMemoryType,
                     lSnapshot.flashCapacityId) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsoleRebootHandler(uint32_t transport, int argc, char *argv[])
{
    uint32_t lFlushCount;

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

bool systemDebugConsoleRegister(void)
{
#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1)
    if (!consoleRegisterCommand(&gSystemVersionConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemStatusConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemRebootConsoleCommand)) {
        return false;
    }

    if (!drvSpiDebugConsoleRegister()) {
        return false;
    }

    if (!gd25qxxxDebugConsoleRegister()) {
        return false;
    }
#endif

    return true;
}

/**************************End of file********************************/
