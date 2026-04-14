/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_system.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_system.h"
#include "usart.h"

volatile Systeam_Mode_EnumDef eSystemMode = E_SYSTEM_STANDBY_MODE;
volatile Device_Err_Typedef g_Err;
volatile uint8_t current_fault_code = 0;  // 当前显示的故障码数字部分（如41）
volatile uint8_t fault_count = 0;         // 当前故障数量
volatile uint8_t current_fault_index = 0; // 当前显示的故障索引

Fault_Info fault_table[] = {
    // CPR模块 (Byte6)
    {1, 6, 0x02, 0},  // BIT1: 加速度计初始化失败 - E01
    {2, 6, 0x04, 0},  // BIT2: RTC自检错误 - E02
    
    // 电源模块 (Byte7)
    {11, 7, 0x02, 1}, // BIT1: 3.3V电源过高 - E11
    {12, 7, 0x04, 1}, // BIT2: 3.3V电源过低 - E12
    {13, 7, 0x08, 1}, // BIT3: 5V电源过高 - E13
    {14, 7, 0x10, 1}, // BIT4: 5V电源过低 - E14
    {15, 7, 0x20, 1}, // BIT5: DC电压输入过高 - E15
    
    // 音频模块 (Byte8)
    {21, 8, 0x02, 2}, // BIT1: 音频模块通信失败 - E21
    {22, 8, 0x04, 2}, // BIT2: 音频模块歌曲数目异常 - E22
    
    // 无线模块 (Byte9)
    {31, 9, 0x02, 3}, // BIT1: 无线模块初始化失败 - E31
    
    // 存储模块 (Byte10)
    {41, 10, 0x02, 4}, // BIT1: 存储模块初始化失败 - E41
};

/**
 * @brief 检测当前活动的故障码
 * @param active_faults 输出参数，存储检测到的故障码数组
 * @return 检测到的故障码数量
 */
uint8_t detect_active_faults(uint8_t active_faults[]) {
    uint8_t count = 0;
    uint8_t byte_value = 0;
    for (uint8_t i = 0; i < MAX_FAULTS; i++) {     
        switch (fault_table[i].module) {
            case 0: byte_value = g_Err.Cpr.byte; break;
            case 1: byte_value = g_Err.Power.byte; break;
            case 2: byte_value = g_Err.Audio.byte; break;
            case 3: byte_value = g_Err.Wireless.byte; break;
            case 4: byte_value = g_Err.Memory.byte; break;
        }    
        if (byte_value & fault_table[i].bit_mask) {
            active_faults[count] = fault_table[i].fault_code;
            count++;
        }
    }
    
    return count;
}

/**
 * @brief 故障码显示处理函数
 *        轮换显示当前所有激活的故障码
 */
uint8_t Err_Code_Show_handle(void) 
{
    static uint8_t active_faults[MAX_FAULTS];
    static uint8_t last_fault_count = 0;

    fault_count = detect_active_faults(active_faults);

    if (fault_count == 0) {
        current_fault_code = NO_FAULT_CODE;
        current_fault_index = 0;
    } else {
        if (fault_count != last_fault_count) {
            current_fault_index = 0;
            last_fault_count = fault_count;
        } else {
            current_fault_index = (current_fault_index + 1) % fault_count;
        }
        current_fault_code = active_faults[current_fault_index];
    }
    return current_fault_code;
}
/**
* @brief SystemModeManager
* @retval None
**/
void SystemModeManager(void)
{
    switch(eSystemMode)
    {
      case E_SYSTEM_STANDBY_MODE:
        // Handle standby mode
		    eSystemMode = E_SYSTEM_NORMAL_MODE;
        break;
      
      case E_SYSTEM_NORMAL_MODE:
        // Handle normal mode
        break;

      case E_SYSTEM_UPDATE_MODE:
        // Handle update mode
        break;

      case E_SYSTEM_FACTORY_MODE:
        // Handle factory mode
        break;

      default:
        // Handle unexpected mode
        break;
    }
}

void System_Progress()
{

}

/**************************End of file********************************/


