/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_audio.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_audio.h"
#include "app_power.h"
#include "app_i2c.h"
#include "app_system.h"

static Audio_List_EnumDef Play_Audio;
Audio_Language_EnumDef Audio_Language_Rev = AUDIO_Default;
Audio_Language_EnumDef Audio_Language = AUDIO_ZH;
uint8_t Audio_Volume_Rev = 0xFF;
uint8_t Audio_Volume = 31;
uint8_t Audio_Change_Language_Flag = 0;
static uint8_t sucReciveData[32];
static uint8_t SendData[8];


extern osMessageQueueId_t Play_DiDIHandle;
extern osMessageQueueId_t Play_AudioHandle;
extern osEventFlagsId_t xSystemEventsHandle;   
extern osMutexId_t xModeMutexHandle; 
extern PowerDown_State_StructDef Power_State;

const char VoiceName[6][10][3] = {
	{"Z0", "Z1", "Z2", "Z3", "Z4", "Z5", "Z6", "Z7", "Z8", "Z9"},
    {"Z0", "Z1", "Z2", "Z3", "Z4", "Z5", "Z6", "Z7", "Z8", "Z9"},
    {"E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9"},
    {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9"},
    {"F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9"},
    {"I0", "I1", "I2", "I3", "I4", "I5", "I6", "I7", "I8", "I9"},
};
/*!
 * \brief 解析数据处理
 * \param   data：解析出来的数据内容，
 *          len: 解析出来的数据长度
 * \return none
*/
static void Audio_RxData_Handle(uint8_t *data,uint8_t len)
{
    switch (data[2])
    {
        case CMD_CHECK_VERSION:
            if(data[1] == 0x18)
            {
                memcpy(Audio_Info.Soft_Version,data+3,21);
            }
            break;
        case CMD_CHECK_VOLUM_SET:
            Audio_Info.Volume = data[3];
            break;
        case CMD_CHECK_STATE:
            Audio_Info.State = data[3];
            break;
        case CMD_CHECK_MUSIC_NUM:
            if(data[1] == 0x05)
            {
                Audio_Info.Music_Cnt = (data[3] << 8) |  data[4];
            }
            break;
        case CMD_CHECK_CONNECT_STATE:
            Audio_Info.Connect = data[3];
            break;
        default:
            break;
    }
}


/*!
 * \brief 音频数据接收
 * \param   data：解析出来的数据内容，
 *          len: 解析出来的数据长度
 * \return none
*/
uint8_t Audio_DataHandle(uint8_t *reply_cmd)
{
	uint8_t Cbuff_Len;
    uint8_t ret = 0;
      
    Cbuff_Len = CBuff_GetLength(&Ring_AudioUart);
    if(Cbuff_Len < 5)
    {
		return ret;
	}
    CBuff_Read(&Ring_AudioUart,sucReciveData,1);
    if(sucReciveData[0] != AUDIO_HEAD)
	{
		CBuff_Pop(&Ring_AudioUart,sucReciveData,1);	
		return ret;
	}
    else
    {
		CBuff_Read(&Ring_AudioUart,sucReciveData,2);
        Ring_AudioUart.HandleDataLength = sucReciveData[1]+2;

        if(Ring_AudioUart.HandleDataLength > 64)		
        {		
            CBuff_Pop(&Ring_AudioUart,sucReciveData,1); 	// detect if the length over the range
            return ret;
        }
        else if(CBuff_GetLength(&Ring_AudioUart) < Ring_AudioUart.HandleDataLength)
        {
            Ring_AudioUart.HandleDataOverTime++;
            if(Ring_AudioUart.HandleDataOverTime >= 20)   // 5ms*20 = 100ms
            {
                Ring_AudioUart.HandleDataOverTime = 0;
                CBuff_Pop(&Ring_AudioUart,sucReciveData,1);				// 100ms don't receive the rest of the data, remove header and return
            }
            return ret;
        }
        else
        {
            Ring_AudioUart.HandleDataOverTime = 0;
            CBuff_Read(&Ring_AudioUart,sucReciveData,Ring_AudioUart.HandleDataLength);			// Pull out all the data        	
            if(sucReciveData[Ring_AudioUart.HandleDataLength-1] != AUIDO_TAIL)  // Check the end flag
            {
                CBuff_Pop(&Ring_AudioUart,sucReciveData,1);  // Remove the head
                return ret;
            }
            else
            {
                CBuff_Pop(&Ring_AudioUart,sucReciveData,Ring_AudioUart.HandleDataLength);
                *reply_cmd = sucReciveData[2];
                Audio_RxData_Handle(sucReciveData,Ring_AudioUart.HandleDataLength);
                ret = 1;
                return ret;                   
            }
        }
    }
}

uint8_t Audio_Send_And_Check(uint8_t cmd,uint8_t *data,uint8_t len,uint16_t wait_ms)
{
    uint32_t tickstart = HAL_GetTick();
    uint8_t recv_cmd;
    CBuff_Clear(&Ring_AudioUart); 
    Audio_Send_Cmd_Data(cmd,data,len);
    while((HAL_GetTick() - tickstart) < wait_ms)
    {			 
		osDelay(20);
        if(Audio_DataHandle(&recv_cmd) == 1)
        {
            if(recv_cmd == cmd)
            {
                return 1;
            }
        }
    }
    return 0;
}

/*!
* \brief 音频模块初始化
* \param   none
* \return none
*/
uint8_t Audio_Init_Check()
{  
    
    BspAudio_Set(AUDIO_DISABLE);
    osDelay(1050);
#ifdef BAT_TEST
    return 0;
#endif
    Drv_Audio_Init();
    BspAudio_Set(AUDIO_ENABLE);
    osDelay(1050);
    if(Audio_Send_And_Check(CMD_CHECK_VERSION,NULL,0,500) != 1)
    {
        return 2;
    }
    if(Audio_Send_And_Check(CMD_CHECK_MUSIC_NUM,NULL,0,500) != 1)
    {
        return 2;
    }
    if(Audio_Send_And_Check(CMD_CHECK_STATE,NULL,0,500) != 1)
    {
        return 2;
    }
    if(Audio_Send_And_Check(CMD_CHECK_CONNECT_STATE,NULL,0,500) != 1)
    {
        return 2;
    }
    //设置为单曲循环模式
    SendData[0] = SINGLE_MODE;
    if(Audio_Send_And_Check(CMD_PLAY_MODE,SendData,1,500) != 1)
    {
        return 2;
    }
    //设置为DAC输出模式
    SendData[0] = DAC_MODE;
    if(Audio_Send_And_Check(CMD_OUTPUT_MODE_SWICTH,SendData,1,500) != 1)
    {
        return 2;
    }
    osDelay(300);

    if(Audio_Info.Soft_Version[0] == 0){
        g_Err.Audio.bits.Communication_Err = 1;
    }

    if(Audio_Info.Music_Cnt == 0){
        g_Err.Audio.bits.Song_Num_Err = 1;
    }
    return  1;
}

/*!
* \brief 音频模块初始化
* \param   none
* \return none
*/
void Audio_Modle_Init(void)
{
    
  if(Audio_Init_Check() == 2) {
    // Audio init err
    g_Err.Audio.bits.Communication_Err = 1;
  }

  if((g_Err.Audio.byte & 0xFE) != 0) {
    g_Err.Audio.bits.Self_Check_Ok = 0;
  }
  else {
    g_Err.Audio.bits.Self_Check_Ok = 1;
  }
}

/*!
* \brief 音频模块关闭
* \param  none
* \return none
*/
void Audio_ShutDown()
{
    BspAudio_Set(AUDIO_DISABLE);
}


void Update_Play_Setting()
{
    // Update Language
    if(Audio_Language != Audio_Language_Rev) {
        Audio_Language = Audio_Language_Rev;
    }
    if(Audio_Change_Language_Flag == 1){
        Audio_Change_Language_Flag = 0;
        Play_Audio = ADUIO_CHANGE_LANGUAGE;
	    osMessageQueuePut(Play_AudioHandle, &Play_Audio,0,0);
    }
    
    // Update Volume
    if(Audio_Volume != Audio_Volume_Rev) {
        switch(Audio_Volume_Rev) {
            case 0:
                SendData[0] = 0;
                break;
            case 1:
                SendData[0] = 20;
                break;
            case 2:
                SendData[0] = 26;
                break;
            case 3:
                SendData[0] = 31;
                break;
            default:
                SendData[0] = 31;
                break;
        }
        if(Audio_Send_And_Check(CMD_VOLUM_SET,SendData,1,500) != 1)
        {
            // Error Handle
        }
        if(Audio_Send_And_Check(CMD_CHECK_VOLUM_SET,NULL,0,500) != 1)
        {
            // Error Handle
        }
        Audio_Volume = Audio_Volume_Rev;
    }
}

void Play_Notice_Audio()
{
    Audio_Info.State = 0x01;
    while(Audio_Info.State == 0x01) // Not playing
    {
        Audio_Send_And_Check(CMD_CHECK_STATE,NULL,0,500);
    }
    switch(Play_Audio) 
	{
        case ADUIO_STRAT_CPR:
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_STRAT_CPR],2);
            break;
        case ADUIO_PRESS_DEEP:
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_PRESS_DEEP],2);
            break;
        case ADUIO_PRESS_SWALLOW:
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_PRESS_SWALLOW],2);
            break;
        case ADUIO_PRESS_FAST:
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_PRESS_FAST],2);
            break;
        case ADUIO_PRESS_SLOW:
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_PRESS_SLOW],2);
            break;
        case ADUIO_LOW_BATTERY:
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_LOW_BATTERY],2);
            break;
        case ADUIO_BATTERY_DEAD:
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_BATTERY_DEAD],2);
            break;
	    case ADUIO_CHANGE_LANGUAGE:
			memcpy(SendData,VoiceName[Audio_Language][ADUIO_CHANGE_LANGUAGE],2);
			break;
        default:
            break;
    }
    if(Audio_Send_And_Check(CMD_EXTFLASH_NAME_PLAY,SendData,2,500) != 1)
    {
        // Error Handle
    }
    Audio_Info.State = 0x02;
}

void Play_DIDI_Voice()
{
    if(osMessageQueueGet(Play_DiDIHandle, &Play_Audio, NULL, 0) == osOK) {
        Audio_Info.State = 0x01;
        while(Audio_Info.State == 0x01) // Not playing
        {
            Audio_Send_And_Check(CMD_CHECK_STATE,NULL,0,500);
        }
        if(Audio_Info.State != 0x01)
        {
            memcpy(SendData,VoiceName[Audio_Language][ADUIO_DIDI],2);
            if(Audio_Send_And_Check(CMD_EXTFLASH_NAME_PLAY,SendData,2,500) != 1)
            {
                // Error Handle
            }			
        }		           
    }
}

void Play_Start_Voice()
{
    Audio_Info.State = 0x01;
    while(Audio_Info.State == 0x01) // Not playing
    {
        Audio_Send_And_Check(CMD_CHECK_STATE,NULL,0,500);
    }
    memcpy(SendData,VoiceName[Audio_Language][ADUIO_STRAT_CPR],2);
    if(Audio_Send_And_Check(CMD_EXTFLASH_NAME_PLAY,SendData,2,500) != 1)
    {
        // Error Handle
    }
}
/*!
* \brief 音频处理任务
* \param   none
* \return none
*/
void App_Audio_Handle()
{
    static Audio_Play_EnumDef Audio_Play_State;	
	if((Power_State.state == DEV_POWER_ON)&&(Audio_Language_Rev != AUDIO_Default)) {

        Update_Play_Setting();
        switch(Audio_Play_State)
        {
            case AUDIO_PLAY_START:
                Play_Start_Voice();
                Audio_Play_State = AUDIO_PLAY_DIDI;             
                break;
            case AUDIO_PLAY_DIDI:
                if(osMessageQueueGet(Play_AudioHandle, &Play_Audio, NULL, 0) == osOK) {
                    Audio_Play_State = AUDIO_PLAY_NOTICE;
                }
                else{
                    Play_DIDI_Voice();
                    break;
                }
            case AUDIO_PLAY_NOTICE:
                Play_Notice_Audio();
				while(Audio_Info.State == 0x01) // Not playing
				{
					Audio_Send_And_Check(CMD_CHECK_STATE,NULL,0,500);
				}
				Audio_Play_State = AUDIO_PLAY_DIDI;
				osMessageQueueGet(Play_DiDIHandle, &Play_Audio, NULL, 0);
                break;
            default:
                break;
        }		
    }
    else {
        Audio_ShutDown();
    }
}


/**************************End of file********************************/


