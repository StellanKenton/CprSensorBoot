/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_tm1651.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "drv_tm1651.h"
#include "lib_aiic.h"

I2C_GPIO_Operations Tm;


static void Tm_sda_output(void)
{
    ;
}

static void Tm_sda_input(void)
{
    ;
}

static void Tm_scl_output(void)
{
    ;
}

static void Tm_scl_input(void)
{
    ;
}

static void Tm_sda_high(void)
{
    HAL_GPIO_WritePin(TM_DIO_GPIO_PORT, TM_DIO_GPIO_PIN, GPIO_PIN_SET);
}

static void Tm_sda_low(void)
{
    HAL_GPIO_WritePin(TM_DIO_GPIO_PORT, TM_DIO_GPIO_PIN, GPIO_PIN_RESET);
}

static void Tm_scl_high(void)
{
    HAL_GPIO_WritePin(TM_CLK_GPIO_PORT, TM_CLK_GPIO_PIN, GPIO_PIN_SET);
}

static void Tm_scl_low(void)
{
    HAL_GPIO_WritePin(TM_CLK_GPIO_PORT, TM_CLK_GPIO_PIN, GPIO_PIN_RESET);
}

static uint8_t Tm_read_sda(void)
{
    return HAL_GPIO_ReadPin(TM_DIO_GPIO_PORT, TM_DIO_GPIO_PIN);
}

/*!
 * \brief GPIO引脚初始化
 * \param None
 * \return 
*/
static void Tm_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
    
    /*Configure GPIO pin : PB13 */
    GPIO_InitStruct.Pin = TM_DIO_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TM_DIO_GPIO_PORT, &GPIO_InitStruct);
    
    /*Configure GPIO pin : PB12 */
    GPIO_InitStruct.Pin = TM_CLK_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TM_CLK_GPIO_PORT, &GPIO_InitStruct);
    
    HAL_GPIO_WritePin(TM_DIO_GPIO_PORT, TM_DIO_GPIO_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TM_CLK_GPIO_PORT, TM_CLK_GPIO_PIN, GPIO_PIN_SET);
}

void delay_us(uint32_t us)
{
    /*
	for循环实现延时us
	*/
	 uint32_t delay = us * 72/5;
	 do
	 {
	  __NOP();
	 }
	 while (delay --);

}
// 初始化TM1651
void BspTM1651_Init(void)
{
    Tm_gpio_init();
    
    Tm.set_sda_output = Tm_sda_output;
    Tm.set_sda_input = Tm_sda_input;
    Tm.set_scl_output = Tm_scl_output;
    Tm.set_scl_input = Tm_scl_input;
    Tm.sda_high = Tm_sda_high;
    Tm.sda_low = Tm_sda_low;
    Tm.scl_high = Tm_scl_high;
    Tm.scl_low = Tm_scl_low;
    Tm.read_sda = Tm_read_sda;
    Tm.delay_us = delay_us;
    
    i2c_init(&Tm);
    
    TM1651_ClearDisplay();
}


// 显示数字（0-9，10表示关闭）
void TM1651_DisplayNumber(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4)
{
    disp(&Tm, 0xc0, dig1); 	
    disp(&Tm, 0xc1, dig2); 
    disp(&Tm, 0xc2, dig3); 
    //disp(&Tm, 0xc3,dig4); 
}

void TM1651_Show_None()
{
    TM1651_DisplayNumber(11,11,11,0);
}

//显示3位数字
void DisplayNumber_4BitDig(uint16_t Num)
{	
    disp(&Tm, 0xc0,(Num/100)%10);
    disp(&Tm, 0xc1,(Num/10)%10);
    disp(&Tm, 0xc2,Num%10);
    //disp(&Tm, 0xc3,2); 
}


void DisplayNumber_ShowErr(uint16_t Num)
{	
    disp(&Tm, 0xc0,12);
    disp(&Tm, 0xc1,(Num/10)%10);
    disp(&Tm, 0xc2,Num%10);
    //disp(&Tm, 0xc3,2); 
}

// 清空显示
void TM1651_ClearDisplay(void)
{
    TM1651_DisplayNumber(10, 10, 10, 10);
}

/**************************End of file********************************/


