/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_excomm.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_EXCOMM_H
#define APP_EXCOMM_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif
#include "lib_ringbuffer.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_system.h"
#include "usart.h"
#include "lib_comm.h"
#include "app_power.h"
#include "app_feedback.h"
#include "app_memory.h"


#define EXCOMM_TICK 5
typedef union {
    uint16_t byte;  
    struct {
        uint16_t HeartBeat : 1;
        uint16_t Version : 1;
        uint16_t bit2 : 1;
        uint16_t bit3 : 1;
        uint16_t bit4 : 1;
        uint16_t bit5 : 1;
        uint16_t bit6 : 1;
        uint16_t bit7 : 1;
    } bits;
} CommByteUnion;


extern CBuff Ring_UartComm;
extern uint8_t PowerUp_By_Medical;
void App_Excomm_Init(void);
void ExComm_DMA_Recive(void);
void App_Uart_Process(void);
void Excomm_Hearbeat_Detect(CBuff *Ring_Comm);
uint8_t Get_MedicalConnect_Status(void);
#ifdef __cplusplus
}
#endif
#endif  // APP_EXCOMM_H
/**************************End of file********************************/

