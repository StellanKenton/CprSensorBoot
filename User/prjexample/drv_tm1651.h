/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_tm1651.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef DRV_TM1651_H
#define DRV_TM1651_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif
#include "lib_i2c.h"
#include "gpio.h"

#define TM_DIO_GPIO_PIN        GPIO_PIN_12
#define TM_DIO_GPIO_PORT       GPIOC
#define TM_CLK_GPIO_PIN        GPIO_PIN_2
#define TM_CLK_GPIO_PORT       GPIOD

// TM1651 I2C 地址（固定部分）
#define TM1651_I2C_ADDR        0x48 >> 1  // 7位地址 (实际地址可能根据硬件不同需要调整)

// 显示控制命令
#define TM1651_CMD_DISPLAY_OFF 0x80
#define TM1651_CMD_DISPLAY_ON  0x88  // 最后3位控制亮度（0-7）

// 数码管寄存器地址
#define TM1651_DIG1_REG        0x68
#define TM1651_DIG2_REG        0x6A
#define TM1651_DIG3_REG        0x6C
#define TM1651_DIG4_REG        0x6E


void TM1651_Init(I2C_HandleTypeDef *hi2c);
void TM1651_SetBrightness(uint8_t level);
void TM1651_DisplayNumber(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4);
void TM1651_ClearDisplay(void);

//显示4位数字
void DisplayNumber_4BitDig(uint16_t Num);
// 初始化TM1651
void BspTM1651_Init(void);
void TM1651_Show_None(void);
void DisplayNumber_ShowErr(uint16_t Num);
extern void delay_us(uint32_t us);




#ifdef __cplusplus
}
#endif
#endif  // DRV_TM1651_H
/**************************End of file********************************/

