/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_delay.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef DRV_DELAY_H
#define DRV_DELAY_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

//#define OS_PLATFORM 

void Drv_Delay(uint32_t ms);
uint32_t Drv_GetTick(void);
#ifdef OS_PLATFORM
void Drv_osDelay(uint32_t ms);
#endif


#ifdef __cplusplus
}
#endif
#endif  // DRV_DELAY_H
/**************************End of file********************************/

