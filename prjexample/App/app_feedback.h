/*!
 * Copyright (c) 2019, Primedic, Inc. All rights reserved.
 * \file  AppCheck.h
 * \brief 按压效果检测
 * \date  2025.05.22
 * \author Jiang
 *
 */
#ifndef APP_CHECK_H
#define APP_CHECK_H

#include "stm32f1xx_hal.h"
#include "app_audio.h"
#include <stdbool.h>
// 如果使用C++编译器，添加extern "C"兼容性
#ifdef __cplusplus
extern "C" {
#endif

#define CPR_TASK_TICK 25
//#define Y8EMC_TEST
//#define BAT_TEST


typedef enum
{

    TIME_WORLD = 0,
    TIME_POWERUP = 1,
    TIME_REAL = 2,
}CPR_Time_Type_EnumDef;

typedef enum
{
    TIME_UPDATE = 0,
    TIME_GET = 1,
}CPR_Time_Operation_EnumDef;

typedef enum
{
    CPR_EFFECT_IDLE,
    CPR_EFFECT_WELL, 
    CPR_EFFECT_BAD,  
    CPR_INTERRUPT, 
}ENUM_CPR_Effect;

typedef enum
{
    CHECK_NORMAL,   // 正在检测
    CHECK_IDLE,     // 空闲状态
    CHECK_ERROR,    // 传感器错误
}ENUM_CHECK;

typedef enum
{  
    SHOW_NORMAL,
    SHOW_INTERRUPT,
    SHOW_IDLE,
}ENUM_FEEDBACK_SHOW;

typedef struct 
{
    ENUM_CHECK Check;
    ENUM_FEEDBACK_SHOW Show;
    ENUM_CPR_Effect Effect;
}HeartBeat_Feedbck_Typedef;

// 每个数据块为16个字节
typedef struct 
{
    uint8_t type;
    uint8_t Depth;
    uint8_t Freq;
    uint8_t Interval;
    uint32_t TimeStamp;
    uint32_t BootStamp;
    uint8_t Reserve1;
    uint8_t RealseDepth;  
    uint16_t CheckSum;
}CPRFeedbck_Data_Typedef;

// 传输的数据块为8个字节，存储的数据块为16个字节，和cpr数据同步大小
typedef struct
{
    uint8_t type; 
    uint32_t TimeStamp;
    uint8_t Reserve[9];
    uint16_t CheckSum;
}CPRFeedbck_time_Typedef;

typedef struct 
{
    bool Show_Flag;
    bool Ins_Flag;
    int16_t Alarm;

    int16_t Depth;
    int16_t Freq;
    float RealseRatio;
    
    int16_t Depth_Ins;
    int16_t Freq_Ins;
    uint32_t TimeStamp;
    bool Alarm_Flag;   
}CPRFeedbck_ShowData_Typedef;

// 报警时间结构体
typedef struct 
{
    uint8_t Depth_High_Limit;
    uint8_t Depth_Low_Limit;
    uint8_t Freq_High_Limit;
    uint8_t Freq_Low_Limit;
    uint8_t RealseDepth_Low_Limit;
    uint16_t Depth_Alarm_Time;
    uint16_t Freq_Alarm_Time;
    uint16_t RealseDepth_Alarm_Time;
    uint16_t Press_Well_Time;


}CPR_Alarm_Limit_Typedef;

uint8_t CPR_is_Idle(void);
void CPR_FeedBack_Handle(void);
void CPR_FeedBack_Self_Check(void);
void CPR_Init_Handle(void);
uint8_t Get_CPR_State(void);
void Time_IOControl(CPR_Time_Type_EnumDef time_type, CPR_Time_Operation_EnumDef operation, uint32_t *time_value);
#ifdef __cplusplus
}
#endif

#endif // APP_CHECK_H
