/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file drv_blemodule.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "drv_blemodule.h"
#include "drv_delay.h"
#include "lib_ringbuffer.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include "app_memory.h"

CBuff Ring_WireLessComm;
static uint8_t Ring_WireLessCommBuff[RING_BLE_BUFFSIZE];
static uint8_t Ring_WireLessCommRecv[256];
static uint8_t su8_AT_Command[WIRELESS_MAX_LEN]= {0};
static uint8_t BleDMARx_Buffer[RX_BUFFER_SIZE];
static uint8_t MAC_ADRESS[16];
static uint8_t BLE_VER[33];
UART_HandleTypeDef *WirelessComm_Uart;


void WirelessComm_DMA_Recive(void)
{
    if(__HAL_UART_GET_FLAG(WirelessComm_Uart, UART_FLAG_IDLE)) 
	{
		__HAL_UART_CLEAR_IDLEFLAG(WirelessComm_Uart); // 清除IDLE标志
		HAL_UART_DMAStop(WirelessComm_Uart);          // 停止DMA（防止数据被覆盖）
		uint16_t rxLen = RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(WirelessComm_Uart->hdmarx);
        CBuff_Write(&Ring_WireLessComm,BleDMARx_Buffer,rxLen);
		HAL_UART_Receive_DMA(WirelessComm_Uart, BleDMARx_Buffer, RX_BUFFER_SIZE); // 重启接收
	 }
}

DMA_TX_Status Get_DMA_TX_Status(UART_HandleTypeDef *huart)
{
    if (huart->gState == HAL_UART_STATE_READY) {
        return DMA_TX_IDLE;
    } else if (huart->gState == HAL_UART_STATE_BUSY_TX) {
        return DMA_TX_BUSY;
    } else if (huart->ErrorCode != HAL_UART_ERROR_NONE) {
        return DMA_TX_ERROR;
    }
    return DMA_TX_BUSY;
}

/*!
* \brief 无线模块GPIO初始化
* \param   none
* \return none
*/
void Wireless_Gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /*Configure GPIO pin : */
    GPIO_InitStruct.Pin = RESET_WIFI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/*!
* \brief 无线模块复位使能
* \param   none
* \return none
*/
void Wireless_Reset_Enable(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /*Configure GPIO pin : */
    GPIO_InitStruct.Pin = RESET_WIFI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, RESET_WIFI_Pin, GPIO_PIN_RESET);
}

/*!
* \brief 无线模块复位失能
* \param   none
* \return none
*/
void Wireless_Reset_Disable(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /*Configure GPIO pin : */
    GPIO_InitStruct.Pin = RESET_WIFI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
/*!
* \brief 十六进制字符转换为数值
* \param   c：十六进制字符
* \return 转换后的数值，非法字符返回0xFF
*/
uint8_t hexCharToValue(char c)
{
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c - '0');      // 数字0-9
    }
    else if (c >= 'a' && c <= 'f') {
        return (uint8_t)(c - 'a' + 10); // 小写a-f
    }
    else if (c >= 'A' && c <= 'F') {
        return (uint8_t)(c - 'A' + 10); // 大写A-F
    }
    else {
        return 0xFF; // 非法字符
    }
}

void HexChangeChar(uint8_t val, uint8_t *out1, uint8_t *out2)
{
    // 将高4位转换为十六进制字符
    uint8_t highNibble = (val >> 4) & 0x0F;
    *out1 = (highNibble < 10) ? ('0' + highNibble) : ('A' + highNibble - 10);
    
    // 将低4位转换为十六进制字符
    uint8_t lowNibble = val & 0x0F;
    *out2 = (lowNibble < 10) ? ('0' + lowNibble) : ('A' + lowNibble - 10);
}
/*!
* \brief 无线模块串口发送
* \param   pData：发送数据指针，
*          Size: 发送数据长度
* \return none
*/
void Wireless_Uart_Send(const uint8_t *pData, uint16_t Size)
{
    HAL_UART_Transmit_DMA(WirelessComm_Uart, pData, Size);
}

void App_Module_Reset()
{
    CBuff_Init(&Ring_WireLessComm,Ring_WireLessCommBuff,Ring_WireLessCommRecv,RING_BLE_BUFFSIZE);
    Wireless_Gpio_Init();
    Wireless_Reset_Enable();
    Drv_Delay(100);  // 等待无线模块复位
    Wireless_Reset_Disable();
    Drv_Delay(1000);
    WirelessComm_Uart = &huart4;
    HAL_UART_MspInit(WirelessComm_Uart);
    __HAL_UART_ENABLE_IT(WirelessComm_Uart, UART_IT_IDLE);
    HAL_UART_Receive_DMA(WirelessComm_Uart, BleDMARx_Buffer, RX_BUFFER_SIZE); 
}

/*!
* \brief 无线模块AT命令检测
* \param   Ring_Comm：接收环形缓冲区指针，
*          cmd：AT命令字符串指针，
*          wait_time: 等待响应时间，单位ms
*          out: 期望的响应类型
* \return 响应结果枚举
*/
WireLess_Recv_EnumDef WireLess_Modlue_AT_Check(CBuff *Ring_Comm,char *cmd, uint16_t wait_time,WireLess_Recv_EnumDef out)
{
    WireLess_Recv_EnumDef ret = wRecv_None;
    uint16_t Send_Len, Recv_Len;
    uint16_t Wait_Cnt = wait_time/5;
    
    Send_Len = strlen((char *)cmd);
    CBuff_Clear(Ring_Comm);
    for(uint8_t j = 0; j < 3; j++) {
        Wireless_Uart_Send((uint8_t *)cmd, Send_Len);     
        for(uint8_t i = 0; i < Wait_Cnt; i++)   // 10ms
        {
            Drv_Delay(5);
            Recv_Len = (uint16_t)CBuff_GetLength(Ring_Comm);
            if(Recv_Len >= 4) {				
                for(uint16_t j = 0; j < Recv_Len - 3; j++) {
					Drv_Delay(5);
					Recv_Len = (uint16_t)CBuff_GetLength(Ring_Comm);
                    CBuff_Read(Ring_Comm, Ring_Comm->RevData, Recv_Len);
                    if(((Ring_Comm->RevData[0] == '+')&&(Ring_Comm->RevData[1] == 'Q'))
                            ||((Ring_Comm->RevData[0] == 'O')&&(Ring_Comm->RevData[1] == 'K'))
                            ||((Ring_Comm->RevData[0] == 'E')&&(Ring_Comm->RevData[1] == 'R'))
                            ||((Ring_Comm->RevData[0] == 'C')&&(Ring_Comm->RevData[1] == 'O')))
                    {                  
                        for(int i = 0; i <= Recv_Len; i++) {
                            if(Ring_Comm->RevData[i] == BLE_CR) {
                                if(Ring_Comm->RevData[i+1] == BLE_LF) {
                                    CBuff_Pop(Ring_Comm,Ring_Comm->RevData,i+2);
                                    Ring_Comm->RevData[i] = 0x00;
                                    if(Ring_Comm->RevData[0] != '+') {
                                        ret = wRecv_Char;
                                        
                                    }else{
                                        ret = wRecv_Puls;
                                    }
                                    if(out == ret) {
										return ret;
                                    }
                                    else {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else {
                        CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);	
                    }
                }
            }
        }
    }
	return ret;
}


uint8_t App_Ble_Init(void)
{
    uint8_t advData[32];  // 蓝牙广播数据最大31字节
    uint8_t advLen = 0;
    char Device_Name[] = "PRIMEDIC-CPRSensor-";
    
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QRST\r\n",400,wRecv_Char) == wRecv_Char) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
    }
    else {
        return 2;
    }
    Drv_Delay(300);

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QSTASTOP\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEINIT=2\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    Factory_TypeDef *Factory_Info = App_Memory_GetFactoryInfo();
    memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLENAME=PRIMEDIC-CPRSensor-%s\r\n", Factory_Info->Name);
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                        
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSSRV=fe60\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                   
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSCHAR=fe61\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSCHAR=fe62\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADVPARAM=150,150\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                    
    memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    advData[advLen++] = 0;  // 长度
    advData[advLen++] = 0x09;                 // 类型：Complete Local Name
    memcpy(&advData[advLen], Device_Name, sizeof(Device_Name));  // 序列号
    advLen += sizeof(Device_Name);
    memcpy(&advData[advLen-1], Factory_Info->Name, 5);     // 名称数据
    advLen += 4;
	advData[advLen++] = '-';
	memcpy(&advData[advLen], &Factory_Info->Device_SN[9], 5);
	advLen += 4;
    advData[0] = advLen-1;
    snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command),"AT+QBLEADVDATA=%02X", advData[0]);
    for (int i = 1; i < advLen; i++) {
        char temp[5] = {0};
        snprintf(temp, sizeof(temp), "%02X", advData[i]);
        strcat((char *)su8_AT_Command, temp);
    }                      
    strcat((char *)su8_AT_Command, "\r\n");
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADDR?\r\n",400,wRecv_Puls) == wRecv_Puls) {
		Ring_WireLessComm.RevData[9] = 0;
		if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLEADDR") != 0) {
            return 0;
        }
        for(int i = 0; i < 6; i++) {
            MAC_ADRESS[i] = hexCharToValue(Ring_WireLessComm.RevData[10+3*i])<<4;
            MAC_ADRESS[i] += hexCharToValue(Ring_WireLessComm.RevData[11+3*i]);
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QVERSION\r\n",400,wRecv_Puls) == wRecv_Puls) {
        Ring_WireLessComm.RevData[9] = 0;
		if(strcmp((const char*)Ring_WireLessComm.RevData, "+QVERSION") != 0) {
            return 0;
        }
        memcpy(BLE_VER, &Ring_WireLessComm.RevData[10], 33);
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADVSTART\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
    return 1;
}

uint8_t *Drv_GetBleMacAddress(void)
{
    return MAC_ADRESS;
}

/*!
* \brief 无线模块数据处理  
* \param   Ring_Comm：接收环形缓冲区指针
* \return 处理结果，0无效数据，1有效数据，2普通字符串，3带+的字符串
*/
uint8_t Wireless_Handle_Data(CBuff *Ring_Comm)
{
    uint8_t ret = 0;
	uint16_t Cbuff_Len;
    uint16_t u16CRC_Sum=0,u16CRC_Calc=0xFFFF;

    Cbuff_Len = CBuff_GetLength(Ring_Comm);
    if(Cbuff_Len < 4){
		return ret;
	} 
    else if(Cbuff_Len > 32) {
        CBuff_Read(Ring_Comm,Ring_Comm->RevData,32);
        Cbuff_Len = 32;
    }
    CBuff_Read(Ring_Comm,Ring_Comm->RevData,Cbuff_Len);
    if(Ring_Comm->RevData[0] == 0xFA && Ring_Comm->RevData[1] == 0xFC)  
    {
        // Ring_WireLessComm.DataLen = (Ring_Comm->RevData[4])*256+Ring_Comm->RevData[5];
        // // 蓝牙工作模式才按加密的处理     
		// if(WireLess_Work_State == BLE_WORK_STATE) {			
		// 	Ring_WireLessComm.DataLen = (Ring_WireLessComm.DataLen + 15) & ~15; // 取16的倍数
		// }    
        // Ring_Comm->HandleDataLength = Ring_WireLessComm.DataLen+8; //CRC16
		Ring_WireLessComm.DataLen = (Ring_Comm->RevData[4])*256+Ring_Comm->RevData[5];
        Ring_Comm->HandleDataLength = (Ring_Comm->RevData[4])*256+Ring_Comm->RevData[5]+8; //CRC16
		
        if(Ring_Comm->HandleDataLength > 256)
        {
            CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1); 	// detect if the length over the range
            return ret;
        }
        else if(CBuff_GetLength(Ring_Comm) < Ring_Comm->HandleDataLength)
        {
            static uint32_t start_tick = 0;
            uint32_t current_tick = Drv_GetTick();
            
            // 第一次进入或已重置，记录开始时间
            if(Ring_Comm->HandleDataOverTime == 0)
            {
                start_tick = current_tick;
                Ring_Comm->HandleDataOverTime = 1;  // 标记已开始计时
            }
            
            // 检查是否超时（100ms）
            if((current_tick - start_tick) >= 100)
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
                //Ble_Data_Unpack(Ring_Comm->RevData);
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
    else if(((Ring_Comm->RevData[0] == '+')&&(Ring_Comm->RevData[1] == 'Q'))
        ||((Ring_Comm->RevData[0] == 'O')&&(Ring_Comm->RevData[1] == 'K'))
        ||((Ring_Comm->RevData[0] == 'E')&&(Ring_Comm->RevData[1] == 'R'))
        ||((Ring_Comm->RevData[0] == 'C')&&(Ring_Comm->RevData[1] == 'N'))
        ||((Ring_Comm->RevData[0] == 'N')&&(Ring_Comm->RevData[1] == 'O')))
    {
        Cbuff_Len = CBuff_GetLength(Ring_Comm);
        if(Cbuff_Len >= 256) {
            CBuff_Read(Ring_Comm,Ring_Comm->RevData,256);
        }
        else {
            CBuff_Read(Ring_Comm,Ring_Comm->RevData,Cbuff_Len);
        }
        for(int i = 0; i <= Cbuff_Len; i++) {
            if(Ring_Comm->RevData[i] == BLE_CR) {
                if(Ring_Comm->RevData[i+1] == BLE_LF) {
                    Ring_Comm->HandleDataOverTime = 0;
                    CBuff_Pop(Ring_Comm,Ring_Comm->RevData,i+2);
                    Ring_Comm->RevData[i] = 0x00;
                    if(Ring_Comm->RevData[0] != '+') {
                        ret = 2;
                    }
                    else{
                        ret = 3;
                    }
                    
                    return ret;
                }
            }
        }
        if(Ring_Comm->HandleDataOverTime >= 100) {
            Ring_Comm->HandleDataOverTime = 0;
            CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
        }
        Ring_Comm->HandleDataOverTime++;
    }
    else {
        CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
        for(int i = 1; i < Cbuff_Len; i++) {
            if((Ring_Comm->RevData[i] == 0xFA )
            ||(Ring_Comm->RevData[i] == '+')
			||(Ring_Comm->RevData[i] == 'O')
            ||(Ring_Comm->RevData[i] == 'E')
			||(Ring_Comm->RevData[i] == 'C')
            ||(Ring_Comm->RevData[i] == 'N')){
                break;
            }
			else {
				CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
			}
        }
		return ret;
	}
	return ret;
}

UART_HandleTypeDef * Drv_BleModuleGetUart(void)
{
    return WirelessComm_Uart;
}
/**************************End of file********************************/


