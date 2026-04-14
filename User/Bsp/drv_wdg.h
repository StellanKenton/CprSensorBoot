/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_wdg.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef DRV_WDG_H
#define DRV_WDG_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#include "iwdg.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

/* Driver API */
uint8_t Drv_WatchDog_Init(uint8_t data);
void Drv_WatchDogFeed(void);
void Drv_WatchDogResartCheck(void);


#ifdef __cplusplus
}
#endif

#endif  // DRV_WDG_H
/**************************End of file********************************/

