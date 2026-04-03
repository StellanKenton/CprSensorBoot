/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_tca9535.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef DRV_TCA9535_H
#define DRV_TCA9535_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif
#include "stm32f1xx_hal.h"
#define LED_MAX     8

extern uint16_t ShowLed;

#define LED_POWER_RED       TCA9535_Output_Port_15
#define LED_POWER_GREEN     TCA9535_Output_Port_14
#define LED_POWER_BLUE      TCA9535_Output_Port_13

#define LED_PRESS_RED       TCA9535_Output_Port_0
#define LED_PRESS_GREEN     TCA9535_Output_Port_17
#define LED_PRESS_BLUE      TCA9535_Output_Port_16

/******************************************************************************************/
/* TCA9535 IIC地址 */

/* TCA9535 IIC器件地址 */
#define TCA9535_Address             TCA9535_Address_HLL

/* 7位TCA9535 IIC器件地址 */                /* 0100 A2 A1 A0 */
#define TCA9535_Address_LLL         0x20    /* 0100 000 */
#define TCA9535_Address_LLH         0x21    /* 0100 001 */
#define TCA9535_Address_LHL         0x22    /* 0100 010 */
#define TCA9535_Address_LHH         0x23    /* 0100 011 */
#define TCA9535_Address_HLL         0x24    /* 0100 100 */
#define TCA9535_Address_HLH         0x25    /* 0100 101 */
#define TCA9535_Address_HHL         0x26    /* 0100 110 */
#define TCA9535_Address_HHH         0x27    /* 0100 111 */

/******************************************************************************************/


/******************************************************************************************/
/* TCA9535 寄存器映射 */

#define TCA9535_Input_Port0                     0x00    /* 输入寄存器0，可读不可写 */
#define TCA9535_Input_Port1                     0x01    /* 输入寄存器1，可读不可写 */

#define TCA9535_Output_Port0                    0x02    /* 输出寄存器0，可读可写 */
#define TCA9535_Output_Port1                    0x03    /* 输出寄存器1，可读可写 */

#define TCA9535_Polarity_Inversion_Port0        0x04    /* 极性反转寄存器0，可读可写 */
#define TCA9535_Polarity_Inversion_Port1        0x05    /* 极性反转寄存器1，可读可写 */

#define TCA9535_Configuration_Port0             0x06    /* 配置寄存器0，可读可写 */
#define TCA9535_Configuration_Port1             0x07    /* 配置寄存器1，可读可写 */

/******************************************************************************************/


/******************************************************************************************/
/* TCA9535 配置为输出端口 */

#define TCA9535_Output_Port_0           0x0001      /* 端口PA0 */
#define TCA9535_Output_Port_1           0x0002      /* 端口PA1 */
#define TCA9535_Output_Port_2           0x0004      /* 端口PA2 */
#define TCA9535_Output_Port_3           0x0008      /* 端口PA3 */
#define TCA9535_Output_Port_4           0x0010      /* 端口PA4 */
#define TCA9535_Output_Port_5           0x0020      /* 端口PA5 */
#define TCA9535_Output_Port_6           0x0040      /* 端口PA6 */
#define TCA9535_Output_Port_7           0x0080      /* 端口PA7 */

#define TCA9535_Output_Port_10          0x0100      /* 端口PB0 */
#define TCA9535_Output_Port_11          0x0200      /* 端口PB1 */
#define TCA9535_Output_Port_12          0x0400      /* 端口PB2 */
#define TCA9535_Output_Port_13          0x0800      /* 端口PB3 */
#define TCA9535_Output_Port_14          0x1000      /* 端口PB4 */
#define TCA9535_Output_Port_15          0x2000      /* 端口PB5 */
#define TCA9535_Output_Port_16          0x4000      /* 端口PB6 */
#define TCA9535_Output_Port_17          0x8000      /* 端口PB7 */

#define TCA9535_Output_Port_ALL_ON      0xFFFF      /* 全部打开 */
#define TCA9535_Output_Port_ALL_OFF     0x0000      /* 全部关闭 */


void led_light_num(uint8_t num);
void led_off(void);
void led_Init(void);
void led_power_show(uint8_t r, uint8_t g, uint8_t b);
void led_press_show(uint8_t r, uint8_t g, uint8_t b);
extern void delay_us(uint32_t us);




#ifdef __cplusplus
}
#endif
#endif  // DRV_TCA9535_H
/**************************End of file********************************/

