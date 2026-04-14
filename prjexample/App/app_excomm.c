/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_excomm.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_excomm.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "log.h"

#define RX_BUFFER_SIZE 64
#define RING_EXCOMM_BUFFSIZE 1024

UART_HandleTypeDef *ExComm_Uart;
CBuff Ring_UartComm;
uint8_t Ring_UartCommBuff[RING_EXCOMM_BUFFSIZE];
uint8_t Ring_UartCommRecv[RX_BUFFER_SIZE];
uint8_t ExCommRxData[RX_BUFFER_SIZE];
uint16_t rxIndex = 0;
uint16_t Y8Device_Heratbeat = 60000;
uint8_t PowerUp_By_Medical = 0;

static void Uart_Data_Unpack(uint8_t *Data);
static CommByteUnion RecvFlag;
static uint8_t Excomm_Send_Buff[64];
static Comm_CPR_Data_Typedef cprData;
static Comm_Flash_Data_Typedef flashData;
static CPRFeedbck_Data_Typedef CPR_Data;

extern osMessageQueueId_t CPR_WriteHandle;
extern osMessageQueueId_t CPR_ReadHandle;
extern osMessageQueueId_t Flash_WriteHandle;
extern osMessageQueueId_t Flash_ReadHandle;
extern osMessageQueueId_t CPR_Data_SendHandle;
//extern osMessageQueueId_t CPR_SendDataHandle;
extern PowerDown_State_StructDef Power_State;
extern osMessageQueueId_t EXCPR_Data_SendHandle;
extern uint8_t global_HardVer_Flag;
void App_BleComm_Init(void);


void App_ExUart_Init(void)
{
    if(global_HardVer_Flag == 3){
       //ExComm_Uart = &huart3;
    }else{
//       ExComm_Uart = &huart1;
    }
	HAL_UART_MspInit(ExComm_Uart);
	__HAL_UART_ENABLE_IT(ExComm_Uart, UART_IT_IDLE);
    HAL_UART_Receive_DMA(ExComm_Uart, ExCommRxData, RX_BUFFER_SIZE);
}

void App_Excomm_Init(void)
{ 
	CBuff_Clear(&Ring_UartComm);
	CBuff_Init(&Ring_UartComm,Ring_UartCommBuff,Ring_UartCommRecv,RING_EXCOMM_BUFFSIZE);
	App_ExUart_Init();
}

void ExComm_DMA_Recive(void)
{
    if(__HAL_UART_GET_FLAG(ExComm_Uart, UART_FLAG_IDLE)) 
	{
		__HAL_UART_CLEAR_IDLEFLAG(ExComm_Uart); // 清除IDLE标志
		HAL_UART_DMAStop(ExComm_Uart);          // 停止DMA（防止数据被覆盖）
		uint16_t rxLen = RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(ExComm_Uart->hdmarx);
        CBuff_Write(&Ring_UartComm,ExCommRxData,rxLen);
		
		HAL_UART_Receive_DMA(ExComm_Uart, ExCommRxData, RX_BUFFER_SIZE); // 重启接收
	 }
}


void ExComm_Uart_Send(const uint8_t *pData, uint16_t Size)
{
   HAL_UART_Transmit(ExComm_Uart, pData, Size, 1000);
}

void Send_HeartBeat()
{
	uint8_t Send_Cnt;
    uint16_t CRC16;
	Send_Cnt = 0;
	Excomm_Send_Buff[Send_Cnt++] = 0xAA;
	Excomm_Send_Buff[Send_Cnt++] = 0xB5;
	Excomm_Send_Buff[Send_Cnt++] = 0xFF;
	Excomm_Send_Buff[Send_Cnt++] = E_COMM_HEARTBEAT;
	for(uint8_t i=0;i<sizeof(HeartBeat_Data_Typedef);i++){
		Excomm_Send_Buff[Send_Cnt++] = ((uint8_t *)&HeartBeat_Data)[i];
	}
	Excomm_Send_Buff[2] = Send_Cnt-3;
	CRC16 = Crc16Compute(Excomm_Send_Buff+3, Send_Cnt-3);
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)((CRC16 >> 8) & 0xFF);
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)(CRC16 & 0xFF);         
	ExComm_Uart_Send(Excomm_Send_Buff,Send_Cnt);	
}

void Send_Version()
{
	uint8_t Send_Cnt;
    uint16_t CRC16;
	Send_Cnt = 0;
	Excomm_Send_Buff[Send_Cnt++] = 0xAA;
	Excomm_Send_Buff[Send_Cnt++] = 0xB5;
	Excomm_Send_Buff[Send_Cnt++] = 0xFF;
	Excomm_Send_Buff[Send_Cnt++] = E_COMM_VERSION;
	Excomm_Send_Buff[Send_Cnt++] = SoftWare_Version;
    Excomm_Send_Buff[Send_Cnt++] = SoftBuild_Version;
    Excomm_Send_Buff[Send_Cnt++] = HardWare_Version;
	Excomm_Send_Buff[2] = Send_Cnt-3;
	CRC16 = Crc16Compute(Excomm_Send_Buff+3, Send_Cnt-3);
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)((CRC16 >> 8) & 0xFF);
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)(CRC16 & 0xFF);         
	ExComm_Uart_Send(Excomm_Send_Buff,Send_Cnt);	
}


void Send_Memory_Data()
{
	uint8_t Send_Cnt;
    uint16_t CRC16;
	Send_Cnt = 0;
	Excomm_Send_Buff[Send_Cnt++] = 0xAA;
	Excomm_Send_Buff[Send_Cnt++] = 0xB5;
	Excomm_Send_Buff[Send_Cnt++] = 0xFF;
	Excomm_Send_Buff[Send_Cnt++] = E_COMM_FLASH;
	Excomm_Send_Buff[Send_Cnt++] = flashData.Cmd;
	for(uint8_t i=0;i<flashData.length;i++){
		Excomm_Send_Buff[Send_Cnt++] = flashData.Data[i];
	}
	Excomm_Send_Buff[2] = Send_Cnt-3;
	CRC16 = Crc16Compute(&Excomm_Send_Buff[3], Send_Cnt-3);
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)((CRC16 >> 8) & 0xFF); 
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)(CRC16 & 0xFF); 
	ExComm_Uart_Send(Excomm_Send_Buff,Send_Cnt);
}

void Send_CPR_Data()
{
	uint8_t Send_Cnt;
    uint16_t CRC16;
	Send_Cnt = 0;
	Excomm_Send_Buff[Send_Cnt++] = 0xAA;
	Excomm_Send_Buff[Send_Cnt++] = 0xB5;
	Excomm_Send_Buff[Send_Cnt++] = 0xFF;
	Excomm_Send_Buff[Send_Cnt++] = E_COMM_CPR;
	Excomm_Send_Buff[Send_Cnt++] = CPR_Data.type;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Depth>>8;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Depth;
	Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Freq>>8;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Freq;
	Excomm_Send_Buff[2] = Send_Cnt-3;
	CRC16 = Crc16Compute(&Excomm_Send_Buff[3], Send_Cnt-3);
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)((CRC16 >> 8) & 0xFF); 
	Excomm_Send_Buff[Send_Cnt++] = (uint8_t)(CRC16 & 0xFF); 
	ExComm_Uart_Send(Excomm_Send_Buff,Send_Cnt);
}

void Send_CPR_Data_To_MedDevice()
{
	static uint16_t Tick;
	Tick += 500;
	
	uint8_t Send_Cnt;
    uint16_t CRC16;
	Send_Cnt = 0;
    Excomm_Send_Buff[Send_Cnt++] = 0xFA;
    Excomm_Send_Buff[Send_Cnt++] = 0xFC;
    Excomm_Send_Buff[Send_Cnt++] = 0x01; 
    Excomm_Send_Buff[Send_Cnt++] = E_CMD_CPR_DATA;   // CMD
    Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.TimeStamp>>24;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.TimeStamp>>16;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.TimeStamp>>8;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.TimeStamp&0x000000FF;
	
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Freq >> 8;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Freq & 0xFF;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Depth;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.RealseDepth;
    Excomm_Send_Buff[Send_Cnt++] = CPR_Data.Interval;
    /***************************************** */
    Excomm_Send_Buff[4] = (Send_Cnt-6) / 256;
    Excomm_Send_Buff[5] = (Send_Cnt-6) % 256; 
    CRC16 = Crc16Compute(&Excomm_Send_Buff[3], (Send_Cnt - 3)); 
    Excomm_Send_Buff[Send_Cnt++] = (CRC16 >> 8) & 0xFF;
    Excomm_Send_Buff[Send_Cnt++] = CRC16 & 0xFF;

    ExComm_Uart_Send(Excomm_Send_Buff,Send_Cnt);
}

void App_Excomm_SendHandle(void)
{
	
	if(osMessageQueueGet(EXCPR_Data_SendHandle, &CPR_Data, NULL, 0) == osOK) {
		Send_CPR_Data_To_MedDevice();
		return;
	}
	
    if(osMessageQueueGet(Flash_ReadHandle, &flashData, NULL, 0) == osOK) {
        Send_Memory_Data();
    }

    if(RecvFlag.bits.HeartBeat == 1) {
        RecvFlag.bits.HeartBeat = 0;
		Send_HeartBeat();
    }
	
	if(RecvFlag.bits.Version == 1) {
        RecvFlag.bits.Version = 0;
		Send_Version();
    }

    // if(osMessageQueueGet(CPR_SendDataHandle, &CPR_Data, NULL, 0) == osOK) {
    //     Send_CPR_Data();
    // }
	
	
}

static void Uart_Data_Unpack(uint8_t *Data)
{
	
    switch(Data[3])
    {
        case E_COMM_DEV_INFO:
			RecvFlag.bits.Version = 1;
            break;
        case E_COMM_HEARTBEAT:
            RecvFlag.bits.HeartBeat = 1;
            break;
        case E_COMM_FLASH:
            flashData.Cmd = Data[4];
            switch(flashData.Cmd)
            {
                case FLASH_READ:
                    flashData.Addr = (Data[5] << 24) | (Data[6] << 16) | (Data[7] << 8) | Data[8];
                    flashData.length = Data[9];
                    osMessageQueuePut(Flash_WriteHandle, &flashData,0,0);
                    break;
                case FLASH_WRITE:
                    flashData.Addr = (Data[5] << 24) | (Data[6] << 16) | (Data[7] << 8) | Data[8];
                    flashData.length = Data[9];
                    for(uint8_t i=0;i<flashData.length;i++){
                        flashData.Data[i] = Data[10+i];
                    }
                    osMessageQueuePut(Flash_WriteHandle, &flashData,0,0);
                    break;
                default:
                    break;
            }          
            break;
        case E_COMM_CPR:         
            cprData.Cmd = 1;
            cprData.Strength = Data[4];
            cprData.Freq = (Data[5] << 24) | (Data[6] << 16) | (Data[7] << 8) | Data[8];
            osMessageQueuePut(CPR_WriteHandle, &cprData,0,0);
            break; 
        default:
            break;
    }
}


uint8_t App_Excomm_Process(CBuff *Ring_Comm)
{
	uint8_t Cbuff_Len;
    uint8_t ret = 0;
    uint16_t u16CRC_Sum=0,u16CRC_Calc=0xFFFF;
	
    Cbuff_Len = CBuff_GetLength(Ring_Comm);
    if(Cbuff_Len < 5)
    {
		return ret;
	}
    CBuff_Read(Ring_Comm,Ring_Comm->RevData,1);
    if(Ring_Comm->RevData[0] != 0xAA)
	{
		CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);	
		return ret;
	}
    else
    {
		CBuff_Read(Ring_Comm,Ring_Comm->RevData,3);  
        Ring_Comm->HandleDataLength = Ring_Comm->RevData[2]+5; //CRC16
        if(Ring_Comm->HandleDataLength > 64)
        {		
            CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1); 	// detect if the length over the range
            return ret;
        }
        else if(CBuff_GetLength(Ring_Comm) < Ring_Comm->HandleDataLength)
        {
            Ring_Comm->HandleDataOverTime++;
            if(Ring_Comm->HandleDataOverTime >= 20)   // 5ms*20 = 100ms
            {
                Ring_Comm->HandleDataOverTime = 0;
                CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);				// 100ms don't receive the rest of the data, remove header and return
            }
            return ret;
        }
        else
        {
            Ring_Comm->HandleDataOverTime = 0;
            CBuff_Read(Ring_Comm,Ring_Comm->RevData,Ring_Comm->HandleDataLength);			// Pull out all the data   
			u16CRC_Calc = Crc16Compute(Ring_Comm->RevData+3,(Ring_Comm->HandleDataLength-5)); 
            u16CRC_Sum = Ring_Comm->RevData[Ring_Comm->HandleDataLength-2]*256 + Ring_Comm->RevData[Ring_Comm->HandleDataLength-1];			
            if(u16CRC_Calc == u16CRC_Sum) {  	
                CBuff_Pop(Ring_Comm,Ring_Comm->RevData,Ring_Comm->HandleDataLength);
                Uart_Data_Unpack(Ring_Comm->RevData);
                ret = 1;
                return ret;
            }
			else
			{
				CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
			}
            return ret; 
        }
    }
}

void Offline_Power_Down()
{
	Y8Device_Heratbeat += EXCOMM_TICK;
	if(Y8Device_Heratbeat >= 60000){
		Y8Device_Heratbeat = 60000;
		if((CPR_is_Idle() == 1)&&(PowerUp_By_Medical == 1)){
            LOG_I("  Device ShutDown by IIC Medical Device Offline\r\n");
			App_Power_ShutDown();
			osDelay(1000);
		}
	}
}

uint8_t Get_MedicalConnect_Status()
{	
    if(Y8Device_Heratbeat >= 60000){	
        return 0;
    }
    else{
        return 1;
    }
}

void Excomm_Hearbeat_Detect(CBuff *Ring_Comm)
{
    if(CBuff_GetLength(Ring_Comm) >= 8){     
        for(int i = 0; i < 8; i++) {
            CBuff_Read(Ring_Comm,Ring_Comm->RevData,1);
            if(Ring_Comm->RevData[0] == 'A'){
                break;
            }
			else{
                CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);  
            }
        }
        CBuff_Read(Ring_Comm,Ring_Comm->RevData,3);
                   if((Ring_Comm->RevData[0] == 'A')&&(Ring_Comm->RevData[1] == 'T')&&(Ring_Comm->RevData[2] == '+')){   
            CBuff_Pop(Ring_Comm,Ring_Comm->RevData,3);
            Y8Device_Heratbeat = 0;
        }
        else{
            CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
        }
    }
}



void App_Uart_Process()
{
	Excomm_Hearbeat_Detect(&Ring_UartComm);
	Offline_Power_Down();
	App_Excomm_SendHandle();
}


/**************************End of file********************************/


