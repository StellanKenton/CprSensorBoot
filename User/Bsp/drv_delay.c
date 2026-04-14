/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_delay.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "drv_delay.h"
#include "stm32f1xx_hal.h"

#ifdef OS_PLATFORM
void Drv_osDelay(uint32_t ms)
{
    osDelay(ms);
}
#endif

void Drv_Delay(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t Drv_GetTick(void)
{
    return HAL_GetTick();
}

/**************************End of file********************************/


