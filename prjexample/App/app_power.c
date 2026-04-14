/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_power.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_power.h"
#include "drv_adc.h"
#include "drv_tca9535.h"
#include "adc.h"
#include "dma.h"
#include "app_excomm.h"
#include "app_system.h"
#include "app_i2c.h"
#include "log.h"

Power_Voltage_TypeDef g_PowerVoltage = {0};
PowerDown_State_StructDef Power_State = {DEV_POWER_ON,0};
uint8_t global_HardVer_Flag;

const Audio_List_EnumDef Play_Low_Bat = ADUIO_LOW_BATTERY;
const Audio_List_EnumDef Play_Bat_Dead = ADUIO_BATTERY_DEAD;

extern osEventFlagsId_t xSystemEventsHandle;   
extern osMutexId_t xModeMutexHandle;  
extern osMutexId_t Led_MutexHandle;
extern osMessageQueueId_t HB_PowerHandle;
extern osMessageQueueId_t Play_AudioHandle;
extern osMutexId_t Power_MutexHandle;
static Power_Led_EnumDef se_Power_State = E_BAT_FULL_MODE;
static uint8_t Bat_CapValue;

void App_Power_CheckPowerBtn(void);
void App_Power_Normal_Handle(void);


static void HeartBeat_Update()
{
	static uint16_t HeartBeat_Tick;
	HeartBeat_Tick += POWER_OS_DELAY;
	if(HeartBeat_Tick >= 500) {
		HeartBeat_Tick = 0;
		osMessageQueuePut(HB_PowerHandle, &g_PowerVoltage,0,0);
	}
}

void Power_Show_LED(uint8_t r, uint8_t g, uint8_t b)
{
	if (osMutexAcquire(Led_MutexHandle, osWaitForever) == osOK) {
		led_power_show(r,g,b);
		osMutexRelease(Led_MutexHandle);
	}
}

static void Power_ShutDown_Check(void)
{
	static uint16_t ShutDownCnt;
	if (!HAL_GPIO_ReadPin(Power_ON_Check_GPIO_Port, Power_ON_Check_Pin)) 
	{
		if (!HAL_GPIO_ReadPin(Power_ON_Check_GPIO_Port, Power_ON_Check_Pin)) {
			ShutDownCnt+=POWER_OS_DELAY;
		}
		else {
			ShutDownCnt-=500;
		}	
	}
	
	 if((ShutDownCnt >= 3000)&&(Get_MedicalConnect_Status() == 0)) {
         LOG_I("Device ShutDown by Power Down Button\r\n");
	 	App_Power_ShutDown();      
	}
		
	if(Power_State.state == DEV_POWER_OFF) {
		osDelay(100);
		Power_Show_LED(0,0,0);
        Power_State.flag.bits.power = 0;
        while(1) {
            if(Power_State.flag.byte == 0) {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);
                LOG_I("Power off system is dead\r\n");
                while(!HAL_GPIO_ReadPin(Power_ON_Check_GPIO_Port, Power_ON_Check_Pin));
                NVIC_SystemReset();
                while(1);
            }
            osDelay(10);
        }
	}
}

void Power_Key_Init()
{
		GPIO_InitTypeDef GPIO_InitStruct = {0};
 
		__HAL_RCC_GPIOC_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();
		/*Configure GPIO pin : PtPin */
		GPIO_InitStruct.Pin = Power_ON_Check_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(Power_ON_Check_GPIO_Port, &GPIO_InitStruct);

		/*Configure GPIO pin : PtPin */
		GPIO_InitStruct.Pin = GPIO_PIN_3;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = BAT_Charging_Status_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(BAT_Charging_Status_GPIO_Port, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = BAT_ChargeDone_Status_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(BAT_ChargeDone_Status_GPIO_Port, &GPIO_InitStruct);
		

}

/**
 * \brief 电源按键检测
 * \param None
 * \return 
*/
void App_Power_CheckPowerBtn()
{
		
    static uint8_t LED_Init_State = 0;
    static uint16_t PowerKeyCnt = 0;
    static uint16_t su16_Charing_Cnt,su16_Charing_Done_CNt,su16_Led_Cnt;
    uint8_t Show_LED=0;
    Power_Key_Init();
    MX_ADC1_Init();
    MX_DMA_Init();
    ADC_Capture_Start();
//    MX_USART1_UART_Init();
    //		MX_USART3_UART_Init();
    App_Excomm_Init();
    HAL_Delay(50);
#if POWER_UP_AUTOMACTICALLY == 1
	Bat_CapValue = 5;
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
    LOG_I("btn skip init starting");
	return;
#endif
		// 多取几次取平均值       
		for(;;)
		{
			if(I2C_GetConnectionStatus() == true){
				// power up
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
				PowerUp_By_Medical = 1;
				return;
			}
			g_PowerVoltage.BAT = Drv_Get_Voltage(DEF_ADC_BAT);
			g_PowerVoltage.DCIN = Drv_Get_Voltage(DEF_ADC_DC);
			// 检测Power_ON_Check_GPIO被按下1s
			if(HAL_GPIO_ReadPin(Power_ON_Check_GPIO_Port, Power_ON_Check_Pin) == GPIO_PIN_RESET)
			{
				PowerKeyCnt+=10;
				if(PowerKeyCnt >= 800) //开机有一些时间，所以稍微不卡那么严格
				{
					if((g_PowerVoltage.DCIN > 470)||(g_PowerVoltage.BAT > (BAT_20perVolt - 5))) {
						HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);    
						HAL_Delay(50);
                        LOG_I("  btn pressed init starting\r\n");
						return;
					}
				}
			}
			else
			{
				PowerKeyCnt = 0;
			}

			if(HAL_GPIO_ReadPin(BAT_Charging_Status_GPIO_Port, BAT_Charging_Status_Pin) == GPIO_PIN_RESET) {
				su16_Charing_Cnt+=10;
				if(su16_Charing_Cnt >= 2000){
					su16_Charing_Cnt = 2000;
					Show_LED = 1;
					if(LED_Init_State == 0) {
							LED_Init_State = 1;
					}
				}
			}
			else{
					su16_Charing_Cnt = 0;
			}

			if(HAL_GPIO_ReadPin(BAT_ChargeDone_Status_GPIO_Port, BAT_ChargeDone_Status_Pin) == GPIO_PIN_RESET) {
				su16_Charing_Done_CNt+=10;
				if(su16_Charing_Done_CNt >= 1500){
						su16_Charing_Done_CNt = 1500;
						Show_LED = 2;
						led_power_show(0,1,0);
						if(LED_Init_State == 0) {
								LED_Init_State = 1;
						}
				}
			}
			else {
					su16_Charing_Done_CNt = 0;
			}
			
			if(LED_Init_State == 1) {
				LED_Init_State = 2;
			}
			else {
				if(Show_LED == 1) {
					su16_Led_Cnt++;
					if(su16_Led_Cnt < 50){
						led_power_show(1,1,0);
					}
					else if(su16_Led_Cnt < 100){
						led_power_show(0,0,0);
					}
					else {
						su16_Led_Cnt =0;
					}
				}
				else if(Show_LED == 2) {
						
				}
			}
			HAL_Delay(10);
		}
}

/*!
* \brief 获取电池电量等级
* \param   none
* \return 电池电量等级，范围0-5
*/
uint8_t Get_Battery_Level(int voltage) 
{
	static uint16_t High_Cnt,Low_Cnt;
	switch(Bat_CapValue) {
	case 0:
		if(voltage >= BAT_20perVolt) {
			High_Cnt+=POWER_OS_DELAY;
		}
		else{
			High_Cnt = 0;
		}
		if((se_Power_State == E_CHARGING_MODE)&&(High_Cnt >= 3000)){
			Bat_CapValue = 1;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
        break;	
    case 1:
		if(voltage >= BAT_40perVolt) {
			High_Cnt+=POWER_OS_DELAY;
			Low_Cnt = 0;
		}
		else if(voltage < BAT_20perVolt) {
			High_Cnt = 0;
			Low_Cnt+=POWER_OS_DELAY;
		}
		else {
			High_Cnt = 0;
			Low_Cnt = 0;
		}
		if((se_Power_State == E_CHARGING_MODE)&&(High_Cnt >= 3000)){
			Bat_CapValue = 2;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
		if((se_Power_State != E_CHARGING_MODE)&&(Low_Cnt >= 10000)){
			Bat_CapValue = 0;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
        break;
    case 2:
		if(voltage >= BAT_60perVolt) {
			High_Cnt+=POWER_OS_DELAY;
			Low_Cnt = 0;
		}else if(voltage < BAT_40perVolt) {
			High_Cnt = 0;
			Low_Cnt+=POWER_OS_DELAY;
		}
		else {
			High_Cnt = 0;
			// 加速进入低电
		}
		if((se_Power_State == E_CHARGING_MODE)&&(High_Cnt >= 3000)){
			Bat_CapValue = 3;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
		if((se_Power_State != E_CHARGING_MODE)&&(Low_Cnt >= 3000)){
			Bat_CapValue = 1;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
        break;
    case 3:
		if(voltage >= BAT_80perVolt) {
			High_Cnt+=POWER_OS_DELAY;
			Low_Cnt = 0;
		}else if(voltage < BAT_60perVolt) {
			High_Cnt = 0;
			Low_Cnt+=POWER_OS_DELAY;
		}
		else {
			High_Cnt = 0;
			Low_Cnt = 0;
		}
		if((se_Power_State == E_CHARGING_MODE)&&(High_Cnt >= 3000)){
			Bat_CapValue = 4;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
		if((se_Power_State != E_CHARGING_MODE)&&(Low_Cnt >= 3000)){
			Bat_CapValue = 2;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
        break;
	case 4:
		if(voltage >= BAT_100perVolt) {
			High_Cnt+=POWER_OS_DELAY;
			Low_Cnt = 0;
		}else if(voltage < BAT_80perVolt) {
			High_Cnt = 0;
			Low_Cnt+=POWER_OS_DELAY;
		}
		else {
			High_Cnt = 0;
			Low_Cnt = 0;
		}
		if((se_Power_State == E_CHARGING_MODE)&&(High_Cnt >= 3000)){
			Bat_CapValue = 5;
			Low_Cnt = 0;
			High_Cnt = 0;
		}
		if((se_Power_State != E_CHARGING_MODE)&&(Low_Cnt >= 3000)){
			Bat_CapValue = 3;
			Low_Cnt = 0;
			High_Cnt = 0;
		}		
        break;
	case 5:
		if(voltage < BAT_100perVolt) {
			Low_Cnt+=POWER_OS_DELAY;
		}else {
			Low_Cnt = 0;
		}
		if(Low_Cnt >= 7000){
			Bat_CapValue = 4;
			Low_Cnt = 0;
		}
        break;
    default:
        break;
	}
	return Bat_CapValue;
}
/*!
* \brief 电源自检
* \param   none
* \return none
*/
void App_Power_Update_Voltage()
{   
	if (osMutexAcquire(Power_MutexHandle, osWaitForever) == osOK) {
		g_PowerVoltage.State = se_Power_State;
		if(g_PowerVoltage.State == E_CHARGING_MODE) {
			g_PowerVoltage.BAT = Drv_Get_Voltage(DEF_ADC_BAT)-15;
		} 
		else {
			g_PowerVoltage.BAT = Drv_Get_Voltage(DEF_ADC_BAT);
		}     
		osMutexRelease(Power_MutexHandle);
	}
	g_PowerVoltage.DCIN = Drv_Get_Voltage(DEF_ADC_DC);
	g_PowerVoltage.Vol_5V0 = Drv_Get_Voltage(DEF_ADC_5V0);
	g_PowerVoltage.Vol_3V3 = Drv_Get_Voltage(DEF_ADC_3V3);
}


uint8_t App_Power_Check_Charging()
{
	static uint16_t su16_Charing_Cnt;
	if(HAL_GPIO_ReadPin(BAT_Charging_Status_GPIO_Port, BAT_Charging_Status_Pin) == GPIO_PIN_RESET) {
        su16_Charing_Cnt+=POWER_OS_DELAY;
        if((su16_Charing_Cnt >= 2000)&&(g_PowerVoltage.DCIN >= 400)){
            su16_Charing_Cnt = 2000;
            return 1;          
        }
    }
    else{
        su16_Charing_Cnt = 0;
    }
	return 0;
}

uint8_t App_Power_Check_ChargeDone()
{
	static uint16_t su16_Charing_Done_CNt;
	if(HAL_GPIO_ReadPin(BAT_ChargeDone_Status_GPIO_Port, BAT_ChargeDone_Status_Pin) == GPIO_PIN_RESET) {
		su16_Charing_Done_CNt+=POWER_OS_DELAY;
		if(su16_Charing_Done_CNt >= 1000){
			su16_Charing_Done_CNt = 0;
			return 1;
		}
	}
	else {
			su16_Charing_Done_CNt=0;
	}
	return 0;
}

uint8_t App_Power_Check_LowBat()
{
	if(g_PowerVoltage.DCIN < 100)
	{
		if(g_PowerVoltage.BatValue < 2){			    
			return 1;
		}
	}
	return 0;
}
/*!
* \brief 电源正常工作模式
* \param   none
* \return none
*/
void App_Power_LED_Handle()
{
		static uint16_t su8_LedTimer;	
		static uint16_t su8_LowBattry_Tick,su_Shutdown_Volt_Cnt;
		static Power_Led_EnumDef se_Power_State_Mem = E_BAT_FULL_MODE;
		su8_LedTimer+=POWER_OS_DELAY;
		if(su8_LedTimer > 1000) {
			su8_LedTimer = 0;
		}
		g_PowerVoltage.BatValue = Get_Battery_Level(g_PowerVoltage.BAT);
		switch(se_Power_State)
		{
			case E_LOW_POWER_MODE:
				if(App_Power_Check_Charging() == 1){
					se_Power_State = E_CHARGING_MODE;
				}
				if(su8_LedTimer < 500) {
					Power_Show_LED(1,0,0);
				} 
				else {
					Power_Show_LED(0,0,0);
				}
				if((g_PowerVoltage.BatValue == 0)&&(g_PowerVoltage.DCIN <= 100)) {
					su_Shutdown_Volt_Cnt+=POWER_OS_DELAY;
				}
				else{
					su_Shutdown_Volt_Cnt = 0;
				}
				if(su_Shutdown_Volt_Cnt>=5000) {
					//关机				
					osMessageQueuePut(Play_AudioHandle, &Play_Bat_Dead,0,0);
					osDelay(5000);
					LOG_I("  Device ShutDown by Low Battery\r\n");
					App_Power_ShutDown();
					osDelay(2000);
				}
				if(su8_LowBattry_Tick == 0) {             
					su_Shutdown_Volt_Cnt =0 ;
					osMessageQueuePut(Play_AudioHandle, &Play_Low_Bat,0,0);
					su8_LowBattry_Tick = 60000; // 1 min   
				}
				su8_LowBattry_Tick-=POWER_OS_DELAY;
				break;
			case E_CHARGING_MODE:
				if(su8_LedTimer < 500) {
					Power_Show_LED(1,1,0);
				} 
				else { 
					Power_Show_LED(0,0,0);
				}
				if((App_Power_Check_Charging() == 0)||(App_Power_Check_ChargeDone() == 1)){
					se_Power_State = E_BAT_FULL_MODE;
				}
				break;
			case E_BAT_FULL_MODE:
				if(App_Power_Check_Charging() == 1){
					se_Power_State = E_CHARGING_MODE;
				}
				if(App_Power_Check_LowBat() == 1){
					se_Power_State = E_LOW_POWER_MODE;
					su8_LowBattry_Tick = 0;
				}
				Power_Show_LED(0,1,0);
				break;
			default:
				break;
		}
	
		if(se_Power_State_Mem != se_Power_State) {
				se_Power_State_Mem = se_Power_State;
		}

}

void App_Hard_Ver_Read()
{
#if POWER_HARD_VER == 0
    static uint8_t Read_Ver_Status;
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = Read_Ver_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(Read_Ver_GPIO_Port, &GPIO_InitStruct);

    for(int i = 0; i <= 10; i++) {
        if(HAL_GPIO_ReadPin(Read_Ver_GPIO_Port, Read_Ver_Pin) == 0) {
            Read_Ver_Status++;
            HAL_Delay(3);
        }
    }
    if(Read_Ver_Status >= 6){
        global_HardVer_Flag = 3;
    }
    else {
        global_HardVer_Flag = 2;
    }
    return;
#endif
    global_HardVer_Flag = POWER_HARD_VER;
}

/**
* @brief Handles the normal power mode of the system.
* This function is called when the system is in normal power mode.
**/
void App_Power_Err_Check()
{
	static uint16_t V5_High_Err_Cnt,V5_Low_Err_Cnt;
	static uint16_t V3v3_High_Err_Cnt,V3v3_Low_Err_Cnt;
	static uint16_t DC_High_Err_Cnt;
	// 5v检测
	if(g_PowerVoltage.Vol_5V0 > 550) {
		V5_High_Err_Cnt += POWER_OS_DELAY;
		V5_Low_Err_Cnt = 0;
		if(V5_High_Err_Cnt >= 3000) {
			g_Err.Power.bits.V5V_High_Err = 1;
			g_Err.Power.bits.V5V_Low_Err = 0;
		}      
	}
	else if(g_PowerVoltage.Vol_5V0 < 450) {
		V5_Low_Err_Cnt += POWER_OS_DELAY;
		V5_High_Err_Cnt = 0;
		if(V5_Low_Err_Cnt >= 3000) {
			g_Err.Power.bits.V5V_Low_Err = 1;
			g_Err.Power.bits.V5V_High_Err = 0;
		}
	}
	else {
		g_Err.Power.bits.V5V_High_Err = 0;
		g_Err.Power.bits.V5V_Low_Err = 0;
		V5_Low_Err_Cnt = 0;
		V5_High_Err_Cnt = 0;
	}
	// 3v3检测
	if(g_PowerVoltage.Vol_3V3 > 350) {
		V3v3_High_Err_Cnt += POWER_OS_DELAY;
		V3v3_Low_Err_Cnt = 0;
		if(V3v3_High_Err_Cnt >= 3000) {
			g_Err.Power.bits.V3V3_High_Err = 1;
			g_Err.Power.bits.V3V3_Low_Err = 0;
		}
	}
	else if(g_PowerVoltage.Vol_3V3 < 310) {
		V3v3_Low_Err_Cnt += POWER_OS_DELAY;
		V3v3_High_Err_Cnt = 0;
		if(V3v3_Low_Err_Cnt >= 3000) {
			g_Err.Power.bits.V3V3_Low_Err = 1;
			g_Err.Power.bits.V3V3_High_Err = 0;
		}
	}
	else {
		g_Err.Power.bits.V3V3_High_Err = 0;
		g_Err.Power.bits.V3V3_Low_Err = 0;
		V3v3_Low_Err_Cnt = 0;
		V3v3_High_Err_Cnt = 0;
	}
	// DC检测
	if(g_PowerVoltage.DCIN > 600){
		DC_High_Err_Cnt += POWER_OS_DELAY;
		if(DC_High_Err_Cnt >= 3000) {
			g_Err.Power.bits.DC_High_Err = 1;
		}
	}
	else {
		DC_High_Err_Cnt = 0;
		g_Err.Power.bits.DC_High_Err = 0;
	}

	// 检测显示结果
	if((g_Err.Power.byte & 0xFE) != 0) {
		g_Err.Power.bits.Self_Check_Ok = 0;
		
	}
	else {
		g_Err.Power.bits.Self_Check_Ok = 1;
	}
}

/*!
* \brief 电源自检
* \param   none
* \return none
*/

void App_Power_Self_Check()
{
	Power_Show_LED(1,1,1);
	osDelay(500);
	for(int i = 0; i < 20; i++) {
		App_Power_Update_Voltage();
		if (g_PowerVoltage.Vol_3V3 > 350) {
			g_Err.Power.bits.V3V3_High_Err = 1;
		}
		if (g_PowerVoltage.Vol_3V3 < 310) {
			g_Err.Power.bits.V3V3_Low_Err = 1;
		}
		if (g_PowerVoltage.Vol_5V0 > 550) {
			g_Err.Power.bits.V5V_High_Err = 1;
		}
		if (g_PowerVoltage.Vol_5V0 < 450) {
			g_Err.Power.bits.V5V_Low_Err = 1;
		}
		if (g_PowerVoltage.DCIN > 600) {
			g_Err.Power.bits.DC_High_Err = 1;
		}
		osDelay(10);
	}
	if((g_Err.Power.byte & 0xFE) != 0) {
		g_Err.Power.bits.Self_Check_Ok = 0;
	}
	else {
		g_Err.Power.bits.Self_Check_Ok = 1;
	}
	
	Power_Show_LED(0,0,0);

	if (g_PowerVoltage.BAT >= BAT_100perVolt) {
		g_PowerVoltage.BatValue =  5;  // ≥4.1V → 5格
		Bat_CapValue = 5;
	} else if (g_PowerVoltage.BAT >= BAT_80perVolt) {
		g_PowerVoltage.BatValue =  4;  // 4.0V~4.09V → 4格
		Bat_CapValue = 4; 
	} else if (g_PowerVoltage.BAT >= BAT_60perVolt) {
		g_PowerVoltage.BatValue =  3;  // 3.8V~3.99V → 3格
		Bat_CapValue = 3;
	} else if (g_PowerVoltage.BAT >= BAT_40perVolt) {
		g_PowerVoltage.BatValue =  2;  // 3.7V~3.79V → 2格
		Bat_CapValue = 2;
	} else if (g_PowerVoltage.BAT >= BAT_20perVolt) {
		g_PowerVoltage.BatValue =  1;  // 3.6V~3.69V → 1格
		Bat_CapValue = 1;
	} else {
		g_PowerVoltage.BatValue =  0;  // <3.6V → 0格
		Bat_CapValue = 0;
	}

	
}

void App_Power_ShutDown(void)
{
    Power_State.state = DEV_POWER_OFF;   
    Power_State.flag.byte = 0;
    Power_State.flag.bits.power = 1;
    Power_State.flag.bits.feedback = 1;
    Power_State.flag.bits.wireless = 1;
    Power_State.flag.bits.memory = 1;
    Power_State.flag.bits.excom = 0;
}

/*!
* \brief 电源管理任务
* \param   none
* \return none
*/
void App_Power_Manager()
{
	App_Power_Update_Voltage();
	App_Power_LED_Handle();
	Power_ShutDown_Check();
	HeartBeat_Update();
}


/**************************End of file********************************/


