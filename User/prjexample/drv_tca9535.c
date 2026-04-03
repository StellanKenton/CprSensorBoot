/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_tca9535.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "drv_tca9535.h"
#include "lib_aiic.h"


uint16_t ShowLed = 0;   // 显示LED(全局)
extern uint8_t global_HardVer_Flag;
extern iic_Node sTCA9535Dev;
uint32_t TCA9535_SCL_Pin;
uint32_t TCA9535_SDA_Pin;
uint32_t TCA9535_RESET_Pin;
GPIO_TypeDef  *TCA9535_SCL_GPIO_Port;
GPIO_TypeDef  *TCA9535_SDA_GPIO_Port;
GPIO_TypeDef  *TCA9535_RESET_GPIO_Port;

/**
 * @brief       读取TCA9535单个寄存器的数据
 * @param       read_address：TCA9535执行读操作时的8位IIC地址
 * @param       reg：要读取的寄存器地址
 * @retval      data：读取到的寄存器数据
 */
uint8_t TCA9535_ReadReg_Data(uint8_t read_address, uint8_t reg)
{
    uint8_t data = 0;                               /* 用于存储读取到的寄存器数据 */
    
    data = Drv_IIC_Read_Reg(&sTCA9535Dev,read_address,reg);
    
    return data;                                    /* 返回读取到的寄存器数据 */
}

/**
 * @brief       向TCA9535单个寄存器写入数据
 * @param       write_address：TCA9535执行写操作时的8位IIC地址
 * @param       reg：要写入的寄存器地址
 * @param       data：要写入的数据
 * @retval      无
 */
void TCA9535_WriteReg_Data(uint8_t write_address, uint8_t reg, uint8_t data)
{
    Drv_IIC_WriteReg(&sTCA9535Dev, write_address, reg, data);
}

/**
 * @brief       读取TCA9535P输出和配置寄存器的数据
 * @param       read_address：TCA9535执行读操作时的8位IIC地址
 * @retval      无
 */
uint32_t TCA9535_ReadAllReg_Data(uint8_t read_address)
{
    uint8_t data1,data2,data3,data4;
    
    data1 = TCA9535_ReadReg_Data(read_address, TCA9535_Output_Port0);              /* 读取TCA9535_Output_Port0寄存器的数据 */
    data2 = TCA9535_ReadReg_Data(read_address, TCA9535_Output_Port1);              /* 读取TCA9535_Output_Port1寄存器的数据 */
    data3 = TCA9535_ReadReg_Data(read_address, TCA9535_Configuration_Port0);       /* 读取TCA9535_Configuration_Port0寄存器的数据 */
    data4 = TCA9535_ReadReg_Data(read_address, TCA9535_Configuration_Port1);       /* 读取TCA9535_Configuration_Port1寄存器的数据 */
    
    return (data1<<24 & data2<<16 & data3<<8 & data4);
}

/**
 * @brief       设置TCA9535配置寄存器0、1
 * @param       write_address：TCA9535执行写操作时的8位IIC地址
 * @param       data：16位数据，每一位代表一个端口的方向，0表示输出，1表示输入
 * @retval      无
 */
void TCA9535_Set_Cfg(uint8_t write_address, uint16_t data)
{
    uint8_t Cfg_data[2];                                                                /* 定义一个2字节的数组，用于存储配置数据 */
    
    Cfg_data[0] = (uint8_t)(data & 0x00FF);                                            /* 获取低8位，并取反存储在Cfg_data[0] */
    Cfg_data[1] = (uint8_t)((data >> 8) & 0x00FF);                                     /* 获取高8位，并取反存储在Cfg_data[1] */
    
    TCA9535_WriteReg_Data(write_address, TCA9535_Configuration_Port0, Cfg_data[0]);     /* 写入配置寄存器Port 0，配置端口的方向 */
    TCA9535_WriteReg_Data(write_address, TCA9535_Configuration_Port1, Cfg_data[1]);     /* 写入配置寄存器Port 1，配置端口的方向 */
}

/**
 * @brief       设置TCA9535输出寄存器0、1
 * @param       write_address：TCA9535执行写操作时的8位IIC地址
 * @param       data：16位数据，每一位代表一个端口的输出状态，0表示低电平，1表示高电平
 * @retval      无
 */
void TCA9535_Set_Output(uint8_t write_address, uint16_t data)
{
    uint8_t output_data[2];                                                             /* 定义一个2字节的数组，用于存储输出数据 */
    
    output_data[0] = (uint8_t)(data & 0x00FF);                                          /* 获取低8位，存储在output_data[0] */
    output_data[1] = (uint8_t)((data >> 8) & 0x00FF);                                   /* 获取高8位，存储在output_data[1] */
    
    TCA9535_WriteReg_Data(write_address, TCA9535_Output_Port0, output_data[0]);         /* 写入输出寄存器Port 0，设置端口的输出状态 */
    TCA9535_WriteReg_Data(write_address, TCA9535_Output_Port1, output_data[1]);         /* 写入输出寄存器Port 1，设置端口的输出状态 */
}

/*!
 * \brief 初始化LED
 * \param None
 * \return 
*/
void led_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    TCA9535_SCL_GPIO_Port = GPIOB;
    TCA9535_SCL_Pin = GPIO_PIN_4;
    TCA9535_SDA_GPIO_Port = GPIOB;
    TCA9535_SDA_Pin = GPIO_PIN_3;
    TCA9535_RESET_GPIO_Port = GPIOA;
    TCA9535_RESET_Pin = GPIO_PIN_15;
	sTCA9535Dev.SCLPort = TCA9535_SCL_GPIO_Port;
	sTCA9535Dev.SCL_Pin = TCA9535_SCL_Pin;
	sTCA9535Dev.SDAPort = TCA9535_SDA_GPIO_Port;
	sTCA9535Dev.SDA_Pin = TCA9535_SDA_Pin;
    __HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
    /*Configure GPIO pin : PtPin */
	
   GPIO_InitStruct.Pin = TCA9535_SCL_Pin;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(TCA9535_SCL_GPIO_Port, &GPIO_InitStruct);
	
   GPIO_InitStruct.Pin = TCA9535_SDA_Pin;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
   HAL_GPIO_Init(TCA9535_SDA_GPIO_Port, &GPIO_InitStruct);

   GPIO_InitStruct.Pin = TCA9535_RESET_Pin;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   HAL_GPIO_Init(TCA9535_RESET_GPIO_Port, &GPIO_InitStruct);

   HAL_GPIO_WritePin(TCA9535_RESET_GPIO_Port, TCA9535_RESET_Pin, GPIO_PIN_SET);
}

/*!
 * \brief 关闭所有LED
 * \param None
 * \return 
*/
void led_off(void)
{
    ShowLed = 0;

    TCA9535_Set_Cfg(TCA9535_Address, 0xffff);
    TCA9535_Set_Output(TCA9535_Address, 0xffff);
}

/*!
 * \brief 点亮一定数量的LED
 * \param None
 * \return 
*/
void led_light_num(uint8_t num)
{

    if(num > LED_MAX) return;

    uint16_t out_data[LED_MAX+1] = {0x0000,0x0100,0x0180,0x01C0,0x01E0,0x01F0,0x01F8,0x01FC,0x01FE};

    // 仅更新“数量灯”相关位(保持电源灯/按压灯状态不变)
    ShowLed &= (uint16_t)~(TCA9535_Output_Port_10 | TCA9535_Output_Port_7 | TCA9535_Output_Port_6 | TCA9535_Output_Port_1 | TCA9535_Output_Port_2 | TCA9535_Output_Port_3 | TCA9535_Output_Port_4 | TCA9535_Output_Port_5);
    ShowLed |= out_data[num];

    uint16_t data = (uint16_t)~ShowLed;

    TCA9535_Set_Cfg(TCA9535_Address, data);
    TCA9535_Set_Output(TCA9535_Address, data);

}

/*!
 * \brief 电源led控制
* \param r:红色；g:绿色； b:蓝色
 * \return 
*/
void led_power_show(uint8_t r, uint8_t g, uint8_t b)
{
    if(r) ShowLed|=(LED_POWER_RED);
    else ShowLed&=(~LED_POWER_RED);

    if(g) ShowLed|=(LED_POWER_GREEN);
    else ShowLed&=(~LED_POWER_GREEN);

    if(b) ShowLed|=(LED_POWER_BLUE);
    else ShowLed&=(~LED_POWER_BLUE);

    uint16_t data = (uint16_t)~ShowLed;

    TCA9535_Set_Cfg(TCA9535_Address, data);
    TCA9535_Set_Output(TCA9535_Address, data);
}

/*!
 * \brief 按压反馈led控制
* \param r:红色；g:绿色； b:蓝色
 * \return 
*/
void led_press_show(uint8_t r, uint8_t g, uint8_t b)
{
    if(r) ShowLed|=(LED_PRESS_RED);
    else ShowLed&=(~LED_PRESS_RED);

    if(g) ShowLed|=(LED_PRESS_GREEN);
    else ShowLed&=(~LED_PRESS_GREEN);

    if(b) ShowLed|=(LED_PRESS_BLUE);
    else ShowLed&=(~LED_PRESS_BLUE);

    uint16_t data = (uint16_t)~ShowLed;

    TCA9535_Set_Cfg(TCA9535_Address, data);
    TCA9535_Set_Output(TCA9535_Address, data);
}



/**************************End of file********************************/


