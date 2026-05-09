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
#include "usb_device.h"

#include "cm_backtrace.h"
#include "console.h"
#include "drv_delay.h"
#include "drv_wdg.h"
#include "log.h"
#include "systask.h"
#include "system_debug.h"
#include "update_mgr.h"

static stSystemManagerState gSystemManagerState;

static bool systemConsoleEnsureReady(void);
static void systemLogPump(void);
static void systemEnsureWatchdogForUpdate(const stUpdateStatus *updateStatus);
static void systemWatchdogProcess(void);
static void systemUsbForceReenumerate(void);

bool systemManagerInit(void)
{
    gSystemManagerState.isInitialized = false;
    gSystemManagerState.isConsoleReady = false;
    gSystemManagerState.isWatchdogActive = false;
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

    (void)systemTaskSchedulerInit();
    (void)updateManagerInit();
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "IWDG deferred until update request is confirmed");
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
    systemUsbForceReenumerate();
    MX_USB_DEVICE_Init();
    (void)systemManagerInit();
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "USB_Select pin is now HIGH, USB should be routed to MCU");
    gSystemManagerState.isInitialized = true;
    systemSetMode(E_SYSTEM_CHECK_MODE);
}

void systemCheckModeRun(void)
{
    if (updateManagerHasNormalAppBootFlag()) {
        (void)updateManagerJumpToAppIfValid();
    }

    systemSetMode(E_SYSTEM_UPDATE_MODE);
}

void systemUpdateModeRun(void)
{
    const stUpdateStatus *lUpdateStatus = updateManagerGetStatus();

    updateManagerProcess(Drv_GetTick());

    lUpdateStatus = updateManagerGetStatus();
    systemEnsureWatchdogForUpdate(lUpdateStatus);
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

static void systemEnsureWatchdogForUpdate(const stUpdateStatus *updateStatus)
{
    if (gSystemManagerState.isWatchdogActive) {
        return;
    }

    if ((updateStatus == NULL) || !updateStatus->isUpdateRequested) {
        return;
    }

    if (Drv_WatchDog_Init(0U) == 0U) {
        gSystemManagerState.isWatchdogActive = true;
        gSystemManagerState.watchdogTick = Drv_GetTick();
        LOG_I(SYSTEM_MANAGER_LOG_TAG,
              "IWDG started for update flow, feed interval=%lu ms",
              (unsigned long)SYSTEM_WDG_FEED_INTERVAL_MS);
        return;
    }

    LOG_W(SYSTEM_MANAGER_LOG_TAG, "IWDG start failed");
}

static void systemWatchdogProcess(void)
{
    uint32_t lNowTick = Drv_GetTick();

    if (!gSystemManagerState.isWatchdogActive) {
        return;
    }

    if ((lNowTick - gSystemManagerState.watchdogTick) < SYSTEM_WDG_FEED_INTERVAL_MS) {
        return;
    }

    Drv_WatchDogFeed();
    gSystemManagerState.watchdogTick = lNowTick;
}

/*
 * Pull USB D+ (PA12) low for a few milliseconds before handing the pins back to
 * the USB peripheral. STM32F103 has no software-controlled D+ pull-up, so after
 * a flash/reset the host may still see the board as connected and skip
 * re-enumeration. Driving D+ low forces the host to observe a disconnect and
 * issue a fresh enumeration when the USB peripheral reasserts the pull-up.
 */
static void systemUsbForceReenumerate(void)
{
    GPIO_InitTypeDef lGpioInit = {0};
    volatile uint32_t lDelay;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Route the external USB mux to the MCU. MX_GPIO_Init leaves this pin
     * in its reset-low state; flip it high here so the USB connector lines
     * are actually presented to the STM32's USB peripheral. */
    HAL_GPIO_WritePin(USB_Select_GPIO_Port, USB_Select_Pin, GPIO_PIN_SET);

    lGpioInit.Pin = GPIO_PIN_12;
    lGpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    lGpioInit.Pull = GPIO_NOPULL;
    lGpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &lGpioInit);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

    /* Hold for roughly 20 ms; HAL tick is not yet fully running here, so use a
     * rough software delay that assumes 72 MHz SYSCLK. */
    for (lDelay = 0U; lDelay < (72000UL * 20UL); ++lDelay) {
        __NOP();
    }

    /* Release the pin so the USB peripheral regains control. */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12);
}
/**************************End of file********************************/
