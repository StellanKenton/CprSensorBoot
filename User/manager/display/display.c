/***********************************************************************************
* @file     : display.c
* @brief    : Display manager implementation.
* @details  : Owns the TM1651 and PCA9535 refresh path used by the scheduler.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "display.h"

#include <stddef.h>

#include "log.h"
#include "pca9535_port.h"
#include "tm1651_port.h"

#define DISPLAY_LOG_TAG                  "SysDisplay"
#define DISPLAY_DEFAULT_BRIGHTNESS       2U

static bool gDisplayManagerReady = false;

void displayManagerReset(void)
{
	gDisplayManagerReady = false;
}

bool displayManagerInit(void)
{
	eDrvStatus lStatus;

	gDisplayManagerReady = false;

	lStatus = tm1651PortInit();
	if (lStatus != DRV_STATUS_OK) {
		LOG_E(DISPLAY_LOG_TAG, "tm1651 init failed, status=%d", (int)lStatus);
		return false;
	}

	lStatus = tm1651PortSetBrightness(DISPLAY_DEFAULT_BRIGHTNESS);
	if (lStatus != DRV_STATUS_OK) {
		LOG_E(DISPLAY_LOG_TAG, "tm1651 brightness failed, status=%d", (int)lStatus);
		return false;
	}

	lStatus = tm1651PortSetDisplayOn(true);
	if (lStatus != DRV_STATUS_OK) {
		LOG_E(DISPLAY_LOG_TAG, "tm1651 display-on failed, status=%d", (int)lStatus);
		return false;
	}

	lStatus = tm1651PortShowNone();
	if (lStatus != DRV_STATUS_OK) {
		LOG_E(DISPLAY_LOG_TAG, "tm1651 clear failed, status=%d", (int)lStatus);
		return false;
	}

	lStatus = pca9535PortInit();
	if (lStatus != DRV_STATUS_OK) {
		LOG_E(DISPLAY_LOG_TAG, "pca9535 init failed, status=%d", (int)lStatus);
		return false;
	}

	lStatus = pca9535PortLedOff();
	if (lStatus != DRV_STATUS_OK) {
		LOG_E(DISPLAY_LOG_TAG, "pca9535 clear failed, status=%d", (int)lStatus);
		return false;
	}

	gDisplayManagerReady = true;
	LOG_I(DISPLAY_LOG_TAG, "display task ready");
	return true;
}

bool displayManagerProcess(const stDisplayManagerInput *input, uint16_t *displayValue)
{
	uint8_t lLedLevel;
	uint16_t lDisplayValue;

	if ((input == NULL) || (displayValue == NULL)) {
		return false;
	}

	if (!gDisplayManagerReady) {
		return false;
	}

	lDisplayValue = (uint16_t)(input->displayValue % 1000U);
	lLedLevel = (uint8_t)((input->heartbeat % PCA9535_PORT_LED_MAX) + 1U);

	if (tm1651PortShowNumber3(lDisplayValue) != DRV_STATUS_OK) {
		gDisplayManagerReady = false;
		LOG_W(DISPLAY_LOG_TAG, "tm1651 refresh lost");
		return false;
	}

	if (pca9535PortLedLightNum(lLedLevel) != DRV_STATUS_OK) {
		gDisplayManagerReady = false;
		LOG_W(DISPLAY_LOG_TAG, "pca9535 numeric led refresh lost");
		return false;
	}

	if (pca9535PortLedPowerShow(!input->isFlashReady, input->isFlashReady, false) != DRV_STATUS_OK) {
		gDisplayManagerReady = false;
		LOG_W(DISPLAY_LOG_TAG, "pca9535 power led refresh lost");
		return false;
	}

	*displayValue = lDisplayValue;
	return true;
}
/**************************End of file********************************/
