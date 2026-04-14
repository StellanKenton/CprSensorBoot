/***********************************************************************************
* @file     : systask.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/

#include "systask.h"

#include "rtos.h"

#include "sysmgr.h"
#include "system.h"
#include "system_debug.h"
#include "../../rep/driver/drvadc/drvadc_debug.h"
#include "../../rep/driver/drvadc/drvadc.h"
#include "../../rep/driver/drvanlogiic/drvanlogiic_debug.h"
#include "../../rep/driver/drviic/drviic_debug.h"
#include "../../rep/service/console/console.h"
#include "../../rep/service/console/log.h"
#include "../manager/power/power.h"
#include "../manager/wireless/wireless.h"
#include "../port/pca9535_port.h"
#include "../port/tm1651_port.h"

#define SYSTASK_LOG_TAG "systask"
#define SYSTASK_STACK_DEPTH_FROM_BYTES(bytes) ((uint32_t)(bytes) / (uint32_t)sizeof(uint32_t))

static bool gSystaskWorkerTasksCreated = false;
static bool gSystaskBackgroundServicesReady = false;

static repRtosTaskHandle gSystemCommTaskHandle = NULL;
static repRtosTaskHandle gSystemMemoryTaskHandle = NULL;
static repRtosTaskHandle gSystemPowerTaskHandle = NULL;
static repRtosTaskHandle gSystemWirelessTaskHandle = NULL;
static repRtosTaskHandle gSystemAudioTaskHandle = NULL;

static const stRepRtosTaskConfig gSystemCommTaskConfig = {
	.name = "commTask",
	.entry = commTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * CommTaskStackSize),
	.priority = CommTaskPriority,
	.handle = &gSystemCommTaskHandle,
};

static const stRepRtosTaskConfig gSystemMemoryTaskConfig = {
	.name = "memorytask",
	.entry = memoryTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * MemoryTaskStackSize),
	.priority = MemoryTaskPriority,
	.handle = &gSystemMemoryTaskHandle,
};

static const stRepRtosTaskConfig gSystemPowerTaskConfig = {
	.name = "powertask",
	.entry = powerTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * PowerTaskStackSize),
	.priority = PowerTaskPriority,
	.handle = &gSystemPowerTaskHandle,
};

static const stRepRtosTaskConfig gSystemWirelessTaskConfig = {
	.name = "wirelessTask",
	.entry = wirelessTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * WirelessTaskStackSize),
	.priority = WirelessTaskPriority,
	.handle = &gSystemWirelessTaskHandle,
};

static const stRepRtosTaskConfig gSystemAudioTaskConfig = {
	.name = "audioTask",
	.entry = audioTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * AudioTaskStackSize),
	.priority = AudioTaskPriority,
	.handle = &gSystemAudioTaskHandle,
};

static bool systaskInitBackgroundServices(void)
{
	if (gSystaskBackgroundServicesReady) {
		return true;
	}

	if (!logInit()) {
		return false;
	}

	if (!consoleInit()) {
		return false;
	}

	if (!systemDebugConsoleRegister()) {
		return false;
	}

	if (!drvAnlogIicDebugConsoleRegister()) {
		return false;
	}

	if (!drvAdcDebugConsoleRegister()) {
		return false;
	}

	if (!drvIicDebugConsoleRegister()) {
		return false;
	}

	gSystaskBackgroundServicesReady = true;
	LOG_I(SYSTASK_LOG_TAG, "background console ready");
	return true;
}


static void commTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		
		(void)repRtosDelayMs(CommTaskInterval);
	}
}

static void memoryTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		
		(void)repRtosDelayMs(MemoryTaskInterval);
	}
}

static void powerTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		
		(void)repRtosDelayMs(PowerTaskInterval);
	}
}

static void wirelessTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		
		(void)repRtosDelayMs(WirelessTaskInterval);
	}
}

static void audioTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		
		(void)repRtosDelayMs(AudioTaskInterval);
	}
}

bool systaskCreateWorkerTasks(void)
{
	if (gSystaskWorkerTasksCreated) {
		return true;
	}

	(void)repRtosTaskCreate(&gSystemCommTaskConfig);
	(void)repRtosTaskCreate(&gSystemMemoryTaskConfig);
	(void)repRtosTaskCreate(&gSystemPowerTaskConfig);
	(void)repRtosTaskCreate(&gSystemWirelessTaskConfig);
	(void)repRtosTaskCreate(&gSystemAudioTaskConfig);

	if ((gSystemCommTaskHandle == NULL) ||
		(gSystemMemoryTaskHandle == NULL) ||
		(gSystemPowerTaskHandle == NULL) ||
		(gSystemWirelessTaskHandle == NULL) ||
		(gSystemAudioTaskHandle == NULL)) {
		return false;
	}

	gSystaskWorkerTasksCreated = true;
	return true;
}

void systemTaskEntry(void *argument)
{
	(void)argument;
	for (;;) {
		systemManagerRun();         // System Manager

        /***************Background Services*********************/
        drvAdcBackground();         // ADC background processing
        if (systaskInitBackgroundServices()) {
			consoleProcess();       // Console processing
		}
        /*******************************************************/
		(void)repRtosDelayMs(SystemTaskInterval);
	}
}

/**************************End of file********************************/
