/*!
 * Copyright (c) 2019, Primedic, Inc. All rights reserved.
 * \file  AppCheck.c
 * \brief 按压效果检测
 * \date  2025.05.22
 * \author Jiang
 *
 */
#include "app_feedback.h"
#include "lis2hh12_cpr_data_retrieve.h"
#include "CprFeedback_C.h"
#include "drv_tca9535.h"
#include "cmsis_os.h"
#include "drv_adc.h"
#include "drv_misc.h"
#include "drv_tm1651.h"
#include "drv_audio.h"
#include "app_power.h"
#include "lib_rtc.h"
#include "app_memory.h"
#include "i2c.h"
#include "app_i2c.h"
#include "app_Wireless.h"
#include "app_excomm.h"
#include "app_system.h"
#include "log.h"
#include "encryption.h"
// 算法原始数据相关
#define ACC_DATA_SAMPLE_PER_INTERVAL 3
static axis3bit16_t rawAxisDataArray[ACC_DATA_SAMPLE_PER_INTERVAL*(FIFO_THRESHOLD_CONFIG+1)];
static volatile uint16_t Force_ADC_Value;
// 算法结果相关
static CprFeedbackRst real_cpr_result;
static CPRFeedbck_ShowData_Typedef s_CPR_Result;
static volatile ENUM_FEEDBACK_SHOW s_FeedBack_Show = SHOW_IDLE;
static volatile ENUM_CPR_Effect CPREffect = CPR_EFFECT_IDLE;
static volatile uint8_t s_Interrupt_Flag = 0;
static CPR_Alarm_Limit_Typedef s_CPR_Alarm_Limit = {
    .Depth_High_Limit = 60,
    .Depth_Low_Limit = 50,
    .Freq_High_Limit = 120,
    .Freq_Low_Limit = 100,
    .RealseDepth_Low_Limit = 20,
    .Depth_Alarm_Time = 15000,
    .Freq_Alarm_Time = 15000,
    .RealseDepth_Alarm_Time = 15000,
    .Press_Well_Time = 15000,
};

// 语音播放
static Audio_List_EnumDef Play_Audio;
uint8_t CPR_Metronome_Freq = 110;
extern osMessageQueueId_t Play_AudioHandle;
extern osMessageQueueId_t Play_DiDIHandle;

// 无线通信
extern osMessageQueueId_t CPR_Data_SendHandle;

// 数据存储
extern uint8_t CPR_Metronome_Freq_Recv;
extern osMessageQueueId_t CPR_Data_SaveHandle;
extern osMessageQueueId_t CPR_Time_SaveHandle;
// IIC通信
extern osMessageQueueId_t EXCPR_Data_SendHandle;
CBuff Ring_CPR_Press_Data;
static uint8_t Ring_CPR_Press_Data_Buff[40];
// 时间相关
extern osMutexId_t Time_SyncHandle;
// 电源
extern PowerDown_State_StructDef Power_State;

// 外部变量
extern osMutexId_t Led_MutexHandle;
extern osMessageQueueId_t HB_FeedbackHandle;
extern Factory_TypeDef Factory_Info;

//CPR算法初始化
static CPR_ALG_CONFIG m_cpr_cfg = 
{
    100,                    //采样率，目前支持 100Hz or 200Hz，对于新的测试工装，200Hz数据准确一些
    8192,                   // 8192 LSB /G
    1.00,                   // 采样率校正系数(有时采样率不准确，导致按压频率和深度出现偏差)
    1.07//8192 / 2,         // 增益校正系数
};
static CPR_ALG_ALARM_LIMIT m_cpr_limit = 
{
    50,     // 按压深度下限，单位 mm
    70,     // 按压深度上限，单位 mm
    100,    // 按压频率下限， 单位 cpm
    120,    // 按压频率上限， 单位 cpm
};


/*!
* \brief 显示LED
* \param   r: 红色亮度 0-1
*          g: 绿色亮度 0-1
*          b: 蓝色亮度 0-1
* \return none
*/
void CPR_Show_LED(uint8_t r, uint8_t g, uint8_t b)
{
    if (osMutexAcquire(Led_MutexHandle, osWaitForever) == osOK) {
        led_press_show(r,g,b);
        osMutexRelease(Led_MutexHandle);
    }
}

// 函数：计算指定年份是否为闰年
static uint8_t is_leap_year(uint16_t year) {
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        return 1;  // 闰年
    }
    return 0;  // 平年
}

// 函数：从2025年1月1日开始的秒数转换为日期时间
static void convert_powerup_time_to_datetime(uint32_t total_seconds, 
                                            uint16_t *year, uint8_t *month, uint8_t *day,
                                            uint8_t *hour, uint8_t *minute, uint8_t *second) {
    // 定义每月的天数（平年）
    const uint8_t month_days_normal[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const uint8_t month_days_leap[] =   {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // 计算当前秒、分、时
    *second = total_seconds % 60;
    total_seconds /= 60;
    *minute = total_seconds % 60;
    total_seconds /= 60;
    *hour = total_seconds % 24;
    total_seconds /= 24;  // 现在total_seconds是从2025年1月1日开始的天数
    
    // 起始年份是2025年
    uint16_t current_year = 2025;
    uint32_t days_count = total_seconds;
    
    // 循环计算年份
    while (1) {
        uint16_t days_in_year = is_leap_year(current_year) ? 366 : 365;
        
        if (days_count < days_in_year) {
            break;  // 找到年份
        }
        
        days_count -= days_in_year;
        current_year++;
    }
    
    *year = current_year;
    
    // 计算月份和日期
    const uint8_t *month_days = is_leap_year(current_year) ? month_days_leap : month_days_normal;
    uint8_t current_month = 1;  // 从1月开始
    uint32_t remaining_days = days_count;  // 当前年份中的第几天（从0开始）
    
    for (current_month = 0; current_month < 12; current_month++) {
        if (remaining_days < month_days[current_month]) {
            *month = current_month + 1;  // 月份（1-12）
            *day = remaining_days + 1;   // 日期（1-31），因为从0开始
            break;
        }
        remaining_days -= month_days[current_month];
    }
}

void print_powerup_time(void) {
    uint32_t PowerUP_Time = GET_RTC_Time();
    
    // 转换为日期时间
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    
    convert_powerup_time_to_datetime(PowerUP_Time, &year, &month, &day, &hour, &minute, &second);
    
    // 输出16进制原始值和转换后的日期时间
    LOG_I("  system Start, PowerUP_Time = 0x%08X", PowerUP_Time);
    LOG_I("  Date: %04u-%02u-%02u %02u:%02u:%02u", 
                     year, month, day, hour, minute, second);
    
}

void Time_IOControl(CPR_Time_Type_EnumDef time_type, CPR_Time_Operation_EnumDef operation, uint32_t *time_value)
{
    uint32_t time_tick = 0;
    static uint32_t PowerUP_Time = 0;
    if(osMutexAcquire(Time_SyncHandle, osWaitForever) == osOK) {
        switch(operation) {
            case TIME_UPDATE:
                
                    switch(time_type) {
                        case TIME_WORLD:
                            SET_RTC_Time(*time_value);
                            break;
                        case TIME_POWERUP:
                            PowerUP_Time = GET_RTC_Time();
                            print_powerup_time();
                            break;
                        case TIME_REAL:
                            portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();
                            break;
                        default:
                            time_tick = 0;
                            break;
                    }
                break;
            case TIME_GET:
                    switch(time_type) {
                        case TIME_WORLD:
                            time_tick = GET_RTC_Time();
                            break;
                        case TIME_POWERUP:
                            time_tick = PowerUP_Time;
                            break;
                        case TIME_REAL:
                            time_tick = portGET_RUN_TIME_COUNTER_VALUE();
                            break;
                        default:
                            time_tick = 0;
                            break;
                    }
                    *time_value = time_tick;
                break;
            default:
                break;
        }
        osMutexRelease(Time_SyncHandle);
    }
}


void CPR_Metronome()
{
	static uint16_t Metronome_Tick;
    static uint16_t Metronome_Value=545;
	Metronome_Tick += CPR_TASK_TICK;
    if(CPR_Metronome_Freq != CPR_Metronome_Freq_Recv){
        CPR_Metronome_Freq = CPR_Metronome_Freq_Recv;
        Metronome_Value = 60000/CPR_Metronome_Freq;
    }

	if(Factory_Info.Type == TYPE_HC610_A || Factory_Info.Type == TYPE_HC630_AB || Factory_Info.Type == TYPE_HCTEST_ABC) {	
		if(Metronome_Tick >= Metronome_Value)
		{
			Play_Audio = ADUIO_DIDI;
			osMessageQueuePut(Play_DiDIHandle, &Play_Audio,0,0);			
		}
#ifdef  Y8EMC_TEST
		if(Metronome_Tick <= 120){
			Motor_TurnOn();	

		}
		else {	
			Motor_TurnOff();
		}
#endif			
	}
	if(Metronome_Tick >= Metronome_Value)
	{
		Metronome_Tick = 0;
	}
}
/*!
* \brief 按压深度显示
* \param   depth: 按压深度，单位mm
* \return none
*/
static void AppShowDepth(uint16_t depth)
{
    uint8_t led = 0;
    if(depth < 10)
        led = 0;
    else if(depth < 20)
        led = 1;
    else if(depth < 30)
        led = 2;
    else if(depth < 40)
        led = 3;
    else if(depth < 50)
        led = 4;
    else if(depth < 55)
        led = 5;
    else if(depth <= 60)
        led = 6;
    else if(depth < 70)
        led = 7;
    else 
        led = 8;
    
    led_light_num(led);
}
//uint16_t cprTick[2]={0}; //测试用
//CPR算法分析
/*!
* \brief 执行CPR算法计算
* \param   pAxisData: 加速度计数据指针
*          count: 数据个数
* \return none
*/
static void performCprCalculate(axis3bit16_t* pAxisData, unsigned char count)
{
  unsigned char index = 0;
  int16_t x,y,z;
  uint32_t Real_Time;

  if(pAxisData == NULL)
    return;
 
  for(index = 0; index < count; index++)
  {
	x = pAxisData[index].i16bit[AXIS_X];
	y = pAxisData[index].i16bit[AXIS_Y];
	z = pAxisData[index].i16bit[AXIS_Z];

    //cprTick[0]++;
    CprFeedback_process(x, y, z, Force_ADC_Value);  //CPR算法分析    
    //cprTick[1]++;
	CprFeedback_get_cpr_rst(&real_cpr_result);
	  
	if(real_cpr_result.SinglePressUpdated == 1)
	{
        Time_IOControl(TIME_REAL, TIME_GET, &Real_Time);
		s_CPR_Result.Show_Flag = true;
        s_CPR_Result.TimeStamp = Real_Time;
        s_CPR_Result.Depth = real_cpr_result.CPRDepth;
        s_CPR_Result.Freq = real_cpr_result.CPRRate;
        s_CPR_Result.RealseRatio = real_cpr_result.CPRRealseRatio; 
        if(s_CPR_Result.Depth >= 80){
            s_CPR_Result.Depth = 80;    
        }
        if(s_CPR_Result.Freq >= 160){                 
            s_CPR_Result.Freq = 160; 
        } else if(s_CPR_Result.Freq < 40) {
            s_CPR_Result.Freq = 40;
            //s_CPR_Result.Depth = 0;
        }
	}
	  
  }
}

/*!
 * \brief 陀螺仪传感器初始化函数
 * \param None
 * \return 
*/
void CPR_Init_Handle(void)
{
	CPRFeedbck_time_Typedef CPR_Time;
	// Initialize the ring buffer
    CBuff_Init(&Ring_CPR_Press_Data,Ring_CPR_Press_Data_Buff,NULL,40);
    // Get RTC time
    Time_IOControl(TIME_WORLD, TIME_GET, &CPR_Time.TimeStamp);
    Time_IOControl(TIME_POWERUP, TIME_UPDATE, NULL);
    CPR_Time.type = BOOT_TIMESTAMP; 
    osMessageQueuePut(CPR_Time_SaveHandle, &CPR_Time, 0, 0);
    // Self check RTC
    if(CPR_Time.TimeStamp <= 0x00010000){
        g_Err.Cpr.bits.RTC_Err = 1;
    }

    // initialize the lis2hh12 sensor
    for(int i=0; i<5; i++)
    {
        if(lis2hh12Init() == 0)
        {
            //初始化cpr算法库
            const char* cprver = CprFeedback_get_version();
            CprFeedback_init(m_cpr_cfg);
            CprFeedback_set_alarmlimit(m_cpr_limit); 		
            s_FeedBack_Show = SHOW_IDLE;
            led_light_num(0);
            TM1651_Show_None();
            if(g_Err.Cpr.bits.RTC_Err == 0){
                g_Err.Cpr.bits.Self_Check_Ok = 1;
            }
            return;
        }
        //把iic引脚配置为输出且拉高，再重新配置
        HAL_I2C_Fix_Init();
        osDelay(10);
        MX_I2C1_Init();
        HAL_I2C_MspInit(&hi2c1);
        osDelay(10);
    }
    g_Err.Cpr.bits.MPU_Init_Err = 1;
}

/*!
 * \brief 深度频率检测函数
 * \param None
 * \return 
*/
static void CPR_Normal_Handle(void)
{
    int32_t status;
    unsigned char sample_cnt = 0;   
    status = lis2hh12DataRetrieve(&sample_cnt, rawAxisDataArray);
	
    /* Perform the CPR algorithm calculation. */
    if((status == ACC_DATA_COMM_SUCCESS)&&(sample_cnt >= 1))
    {
        performCprCalculate(rawAxisDataArray, sample_cnt);      
    }
    else if(status == ACC_DATA_COMM_ERR)
    {
    	//RecoverDataComErr();
    }
    else if(status == ACC_DATA_INVALID)
    {
    	/* 不做任何操作。 */
    }

}

/*!
* \brief 按压效果显示
* \param   freq: 按压频率，单位cpm
*          depth: 按压深度，单位mm
* \return none
*/
void CPR_LED_Handle(uint16_t freq,uint16_t depth)
{
    if(((freq>=100)&&(freq<=120))&&((depth>=50)&&(depth<=60))) {
        CPR_Show_LED(0,1,0);
    }
    else{
        CPR_Show_LED(1,0,0);
    }
                
}

void Motor_Handle(uint8_t cmd,uint8_t depth)
{
    static int16_t Motor_Tick=0;
    if((cmd == 1)&&(depth >= s_CPR_Alarm_Limit.Depth_Low_Limit)
        &&(depth <= s_CPR_Alarm_Limit.Depth_High_Limit)){
        Motor_Tick = 200; 
        Motor_TurnOn();
    }else {
        Motor_Tick -= CPR_TASK_TICK;
        if(Motor_Tick <= 0){
            Motor_TurnOff();
            Motor_Tick = 0;
        }
    }
}

uint8_t CPR_is_Idle()
{
	if(s_FeedBack_Show != SHOW_NORMAL){
		return 1;
	}
	return 0;
}

uint8_t Get_CPR_State()
{
    return (uint8_t)s_FeedBack_Show;
}
   
/*!
* \brief CPR FeedBack Self Check
* \param   none
* \return none
*/
void CPR_FeedBack_Self_Check()
{
    DisplayNumber_4BitDig(888);
    led_light_num(8); 
    osDelay(400);
    led_light_num(0);  
    TM1651_ClearDisplay();
    
    CPR_Show_LED(1,1,1);
    osDelay(400);
    CPR_Show_LED(0,0,0);
	
    Buzzer_TurnOn();
    osDelay(100);
    Buzzer_TurnOff();

    Motor_TurnOn();
    osDelay(100);
    Motor_TurnOff();

    osDelay(5);
}

/*!
* \brief CPR FeedBack Shut Down
* \param   none
* \return none
*/
void FeedBack_ShutDownCheck()
{

    led_light_num(0);
    CPR_Show_LED(0,0,0);
    TM1651_ClearDisplay();
    Buzzer_TurnOff();
    Motor_TurnOff();
    Power_State.flag.bits.feedback = 0;
	while(1) {
		osDelay(5000);
	}
}
/*******************************************/
// 新算法相关内容
/*!
 * \brief 空闲状态
 * \param None
 * \return 
*/
//static void CPR_Time_Save(void)
//{
//    static uint16_t SaveTick = 0;
//    CPRFeedbck_time_Typedef CPR_Time;
//    SaveTick += CPR_TASK_TICK;
//    if(SaveTick < 30000){
//        return;
//    }
//    SaveTick = 0;
//    CPR_Time.type = TIMESTAMP_DATA;
//    Time_IOControl(TIME_WORLD, TIME_GET, &CPR_Time.TimeStamp);
//    osMessageQueuePut(CPR_Time_SaveHandle, &CPR_Time,0,0);
//}

void CPR_Alarm_Handle(uint16_t freq,uint8_t depth,uint8_t Realse,uint32_t intrup_Time)
{
    static uint16_t s_Freq_Mem, s_Depth_Mem;// ,s_Realse_Mem;
    static uint16_t s_Freq_Last, s_Depth_Last;//, s_Realse_Last;
    static uint16_t s_Freq_Now, s_Depth_Now;//, s_Realse_Now;

    static uint16_t Alarm_Tick = 0;
    Alarm_Tick += CPR_TASK_TICK;
    
    s_Freq_Mem = s_Freq_Last;
    s_Depth_Mem = s_Depth_Last;
    //s_Realse_Mem = s_Realse_Last;
    s_Freq_Last = s_Freq_Now;
    s_Depth_Last = s_Depth_Now;
    //s_Realse_Last = s_Realse_Now;
    s_Freq_Now = freq;
    s_Depth_Now = depth;
    //s_Realse_Now = Realse;

    if(intrup_Time >= 3000){ // 中断状态不进行报警判断
        Alarm_Tick -= 3000;
        if(Alarm_Tick < 3000){
            Alarm_Tick = 0;
        }
        return;
    }

    if(Alarm_Tick >= 15000)
    {
        Alarm_Tick = 0;
        // Depth Alarm
        // 如果3次中有2次超出范围，则报警
        if(((s_Depth_Mem > s_CPR_Alarm_Limit.Depth_High_Limit) + (s_Depth_Last > s_CPR_Alarm_Limit.Depth_High_Limit) 
            + (s_Depth_Now > s_CPR_Alarm_Limit.Depth_High_Limit)) >= 2) {
            Play_Audio = ADUIO_PRESS_DEEP;
            osMessageQueuePut(Play_AudioHandle, &Play_Audio,0,0);
        }
        else if(((s_Depth_Mem < s_CPR_Alarm_Limit.Depth_Low_Limit) + (s_Depth_Last < s_CPR_Alarm_Limit.Depth_Low_Limit) 
            + (s_Depth_Now < s_CPR_Alarm_Limit.Depth_Low_Limit)) >= 2) {
            Play_Audio = ADUIO_PRESS_SWALLOW;
            osMessageQueuePut(Play_AudioHandle, &Play_Audio,0,0);
        }
        // Freq Alarm
        if(((s_Freq_Mem > s_CPR_Alarm_Limit.Freq_High_Limit) + (s_Freq_Last > s_CPR_Alarm_Limit.Freq_High_Limit) 
            + (s_Freq_Now > s_CPR_Alarm_Limit.Freq_High_Limit)) >= 2) {
            Play_Audio = ADUIO_PRESS_FAST;
            osMessageQueuePut(Play_AudioHandle, &Play_Audio,0,0);
        }
        else if(((s_Freq_Mem < s_CPR_Alarm_Limit.Freq_Low_Limit) + (s_Freq_Last < s_CPR_Alarm_Limit.Freq_Low_Limit) 
            + (s_Freq_Now < s_CPR_Alarm_Limit.Freq_Low_Limit)) >= 2) {
            Play_Audio = ADUIO_PRESS_SLOW;
            osMessageQueuePut(Play_AudioHandle, &Play_Audio,0,0);
        }
        // Press Well Alarm
        if(((s_Depth_Mem >= s_CPR_Alarm_Limit.Depth_Low_Limit)&&(s_Depth_Mem <= s_CPR_Alarm_Limit.Depth_High_Limit)
            &&(s_Freq_Mem >= s_CPR_Alarm_Limit.Freq_Low_Limit)&&(s_Freq_Mem <= s_CPR_Alarm_Limit.Freq_High_Limit)) +
           ((s_Depth_Last >= s_CPR_Alarm_Limit.Depth_Low_Limit)&&(s_Depth_Last <= s_CPR_Alarm_Limit.Depth_High_Limit)
            &&(s_Freq_Last >= s_CPR_Alarm_Limit.Freq_Low_Limit)&&(s_Freq_Last <= s_CPR_Alarm_Limit.Freq_High_Limit)) +
           ((s_Depth_Now >= s_CPR_Alarm_Limit.Depth_Low_Limit)&&(s_Depth_Now <= s_CPR_Alarm_Limit.Depth_High_Limit)
            &&(s_Freq_Now >= s_CPR_Alarm_Limit.Freq_Low_Limit)&&(s_Freq_Now <= s_CPR_Alarm_Limit.Freq_High_Limit)) >= 2) {
            Play_Audio = ADUIO_PRESS_WELL;
            osMessageQueuePut(Play_AudioHandle, &Play_Audio,0,0);
        }


    }
}

void Trans_Data_To_IIC_Buffer(CPRFeedbck_Data_Typedef* CPR_Data)
{
    // AES encryption is required
    uint8_t CPR_Press_Data[16];
    uint8_t Encryted_Data[16];
    CPR_Press_Data[0] = CPR_Data->BootStamp & 0xFF;
    CPR_Press_Data[1] = (CPR_Data->BootStamp >> 8) & 0xFF;
    CPR_Press_Data[2] = (CPR_Data->BootStamp >> 16) & 0xFF;
    CPR_Press_Data[3] = (CPR_Data->BootStamp >> 24) & 0xFF;
    CPR_Press_Data[4] = CPR_Data->TimeStamp & 0xFF;
    CPR_Press_Data[5] = (CPR_Data->TimeStamp >> 8) & 0xFF;
    CPR_Press_Data[6] = (CPR_Data->TimeStamp >> 16) & 0xFF;
    CPR_Press_Data[7] = (CPR_Data->TimeStamp >> 24) & 0xFF;
    CPR_Press_Data[8] = CPR_Data->Freq;
    CPR_Press_Data[9] = CPR_Data->Depth;
    CPR_Press_Data[10] = CPR_Data->RealseDepth;
    my_aes_encrypt(CPR_Press_Data, Encryted_Data,16);
    CBuff_Write(&Ring_CPR_Press_Data,Encryted_Data,16);
}

void Process_CPR_Raw_Data(uint16_t freq,uint8_t depth,uint8_t Realse,uint32_t Real_Time)
{
    CPRFeedbck_Data_Typedef CPR_Data;

    CPR_Data.type = CPR_DATA;
    CPR_Data.Depth = depth;
    CPR_Data.Freq = freq;
    CPR_Data.RealseDepth = Realse;
    CPR_Data.TimeStamp = Real_Time;
    Time_IOControl(TIME_POWERUP, TIME_GET, &CPR_Data.BootStamp);
    CPR_Data.Interval = 0; //单位秒

        osMessageQueuePut(CPR_Data_SendHandle, &CPR_Data,0,0);
        osMessageQueuePut(EXCPR_Data_SendHandle, &CPR_Data,0,0);
    // save history data
//    if((CPR_Data.Depth != 0)&&(CPR_Data.Freq != 0)){
        osMessageQueuePut(CPR_Data_SaveHandle, &CPR_Data,0,0);
//    } 
    Trans_Data_To_IIC_Buffer(&CPR_Data);
}



void CPR_Raw_Data_Handle()
{
    Force_ADC_Value = Drv_Get_ADC_Value(DEF_ADC_FORCE);
    CPR_Normal_Handle();
}

void CPR_Show_Depth()
{
    static uint16_t CPR_Show_Depth_Tick;
    static uint16_t depth,Depth_Cnt;
    static uint8_t CPR_Show_Depth_State=0;
    CPR_Show_Depth_Tick += CPR_TASK_TICK;
    if(CPR_Show_Depth_Tick%50 == 0)
    {
        switch(CPR_Show_Depth_State) {
            case 0:
                if(Force_ADC_Value <= 1000) {
                    AppShowDepth(1);
                    CPR_Show_Depth_State = 1;
                }
                break;
            case 1:
                AppShowDepth(2);
                if(s_CPR_Result.Show_Flag == true) {
                    depth = real_cpr_result.CPRDepth;
                    CPR_Show_Depth_State = 2;
                    Depth_Cnt = 2;
                }
                break;
            case 2:
                Depth_Cnt++;
                AppShowDepth(Depth_Cnt);
                if(Depth_Cnt >= depth) {
                    CPR_Show_Depth_State = 3;
                    Depth_Cnt = 0;
                }
                break;
            case 3:
                Depth_Cnt++;
                if(Depth_Cnt >2){
                    CPR_Show_Depth_State = 0;
                    Depth_Cnt = 0;
                }
                break;
            default:
                break;
        }
    }
}

void Device_Show_Err_Code() 
{
	static uint16_t Err_Code_Tick;
    uint8_t Show_Err_Code = 0;
    Err_Code_Tick += CPR_TASK_TICK;
    if(Err_Code_Tick>=1000) {
		Show_Err_Code = Err_Code_Show_handle();
		Err_Code_Tick = 0;
        if(Show_Err_Code == 0) {
			TM1651_Show_None();
            CPR_Show_LED(0,0,0);
        } else {
			DisplayNumber_ShowErr(Show_Err_Code);
            CPR_Show_LED(1,1,1);
        }
    }
}

void CPR_FeedBack_FSMachine()
{
    static uint16_t freq, depth, RealseDepth;
    static uint32_t Interrupt_Cnt;

    Interrupt_Cnt += CPR_TASK_TICK;
    CPR_Raw_Data_Handle();
    // Process Output
    switch(s_FeedBack_Show)
    {       
        case SHOW_IDLE:
			Device_Show_Err_Code();	    // 显示错误代码
            if(s_CPR_Result.Show_Flag == true) {
                s_FeedBack_Show = SHOW_NORMAL;
            }
            if(Interrupt_Cnt >= 600000) {
				if(Wireless_Get_Connect_Status() == 1){
					Interrupt_Cnt = 500000;
				}
				else {
					if(Get_MedicalConnect_Status() == 0){
                        LOG_I("  Device ShutDown by CPR Idle\r\n");
						App_Power_ShutDown();
					}
				}
			}
            break;
        case SHOW_NORMAL:
            CPR_Metronome();    // 节拍器    
            /*************CPR Alarm*******************/
            CPR_Alarm_Handle(freq,depth,RealseDepth,Interrupt_Cnt);      
            if(s_CPR_Result.Show_Flag == true) {
                s_CPR_Result.Show_Flag = false;
                freq = s_CPR_Result.Freq;
                depth = s_CPR_Result.Depth;
                RealseDepth = (1.0-s_CPR_Result.RealseRatio)*s_CPR_Result.Depth;
				DisplayNumber_4BitDig(freq);
				AppShowDepth(depth);
                CPR_LED_Handle(freq,depth);
                Interrupt_Cnt = 0;
                Motor_Handle(1,depth); // Turn on the motor according to the depth
                // upload data
                Process_CPR_Raw_Data(freq,depth,RealseDepth,s_CPR_Result.TimeStamp);
            }
            Motor_Handle(0,0);  // Turn off the motor after the time is reached
            if(Interrupt_Cnt >= 1500) {
                TM1651_Show_None();
                AppShowDepth(0);
                CPR_Show_LED(0,0,1);
                s_FeedBack_Show = SHOW_INTERRUPT;              
            }
            break;
        case SHOW_INTERRUPT:
            if(Interrupt_Cnt >= 3000) {
                if(Interrupt_Cnt%1000 == 0) {
                    CPR_Alarm_Handle(freq,depth,RealseDepth,Interrupt_Cnt);   // Clear Alarm Count
                    DisplayNumber_4BitDig(Interrupt_Cnt/1000);
                }
            }
            if(Interrupt_Cnt >= 300000) {
                TM1651_Show_None();
                AppShowDepth(0);
                CPR_Show_LED(0,0,0);
                s_FeedBack_Show = SHOW_IDLE;
            }
            if(s_CPR_Result.Show_Flag == true) {
				TM1651_Show_None();
                CPR_Show_LED(1,0,0);
                s_FeedBack_Show = SHOW_NORMAL;
            }
            break;
        default:
            break;
    }   
}

/*!
 * \brief CPR FeedBack Task
 * \param None
 * \return 
*/
void CPR_FeedBack_Handle()
{  
    if(Power_State.state == DEV_POWER_ON) {  	
        CPR_FeedBack_FSMachine();		
    }
    else {
        FeedBack_ShutDownCheck();
    }
}
/**************************End of file********************************/

