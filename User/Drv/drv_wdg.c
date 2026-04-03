/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_wdg.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "drv_wdg.h"
#include "log.h"

/**
* @brief Initialize the WatchDog timer with the specified configuration.
* @param data Configuration data for the WatchDog timer.
* @return Status of the initialization (0 for success, non-zero for failure).
**/
uint8_t Drv_WatchDog_Init(uint8_t data)
{
	/* data parameter currently unused; use generated MX_IWDG_Init to configure IWDG */
	MX_IWDG_Init();
	return 0;
}


/**
 * @brief Feed (refresh) the independent watchdog to prevent reset.
 */
void Drv_WatchDogFeed(void)
{
	/* Refresh the watchdog counter; if refresh fails attempt re-init */
	if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK) {
		/* Try to re-initialize watchdog as a recovery action */
		MX_IWDG_Init();
	}
}

void Drv_WatchDogResartCheck(void)
{
	/* 检查是否由独立看门狗复位导致开机 */
	if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
		// 由看门狗复位导致
        LOG_E("####################################");
        LOG_E("###System restarted by IWDG reset###");
        LOG_E("####################################");
        /* 清除复位标志 */
		__HAL_RCC_CLEAR_RESET_FLAGS();
	}
    
}

/**************************End of file********************************/


