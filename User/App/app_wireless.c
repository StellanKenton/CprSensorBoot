/** 
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_Wireless.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_Wireless.h"
#include "drv_delay.h"
#include "drv_blemodule.h"
#include "log.h"
#include "encryption.h"
#include "lib_comm.h"
#include "app_system.h"
#include "app_bootloader.h"

static BLE_Mgr_t s_WireLessMgr;
static uint8_t Encrypt_Data[256];
static uint8_t Decrypt_Data[256];
static uint8_t sSendBuffer[WIRELESS_MAX_LEN] = {0};
static uint8_t su8_AT_Command[WIRELESS_MAX_LEN]= {0};
static Ble_Send_ByteUnion  Wireless_SendFlag;
static UART_HandleTypeDef *WirelessComm_Uart;

static Wireless_OTA_RxData_Typedef BleData_OTA_Rx;
static Wireless_OTA_TxData_Typedef BleData_OTA_Tx;

extern AESInfo_t aesInfo;
extern CBuff Ring_WireLessComm;

void WireLess_Init()
{
    s_WireLessMgr.eState = BLE_HARD_INIT;
    s_WireLessMgr.IsConnected = false;
    s_WireLessMgr.isInited = false;
}


void Wireless_ChangeState(BLE_FSM_EnumDef newState)
{
    if(newState != s_WireLessMgr.eState && newState < BLE_STATE_MAX){
        s_WireLessMgr.eState = newState;
    }
}

void Init_MD5_key()
{
    static volatile uint8_t MD5_MAC_KEY[17] = {0};
    uint8_t *MAC_ADRESS = Drv_GetBleMacAddress();
    char Unpack_MD5[33] = {0};
    MD5_To_32Upper((char *)MAC_ADRESS,(char *)Unpack_MD5);
    // 把Unpack_MD5转换为16进制
    for(int i = 0; i < 16; i++) {
        sscanf(&Unpack_MD5[i*2], "%2hhx", &MD5_MAC_KEY[i]);
    }
    aesInfo.key = (const void *)MD5_MAC_KEY;
	my_aes_init();
}

static void Wireless_Send_Common(uint8_t cmd, uint8_t *pData, uint16_t len)
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    
    // Header
    sSendBuffer[0] = 0xFA;
    sSendBuffer[1] = 0xFC;
    sSendBuffer[2] = 0x01; 
    sSendBuffer[3] = cmd;   // CMD
    u16Send_Cnt = 4;
    u16Send_Cnt += 2; // Reserve length bytes

    // Handle Data
    if (pData != NULL && len > 0) {
        uint16_t aligned_len = (len + 15) & ~15;
        if(aligned_len > sizeof(Encrypt_Data)) aligned_len = sizeof(Encrypt_Data); // Safety cap
        
        uint8_t temp_plain[128] = {0}; 
        if(aligned_len > sizeof(temp_plain)) aligned_len = sizeof(temp_plain);
        
        memset(temp_plain, 0, aligned_len);
        memcpy(temp_plain, pData, (len > aligned_len) ? aligned_len : len);
        
        my_aes_encrypt(temp_plain, Encrypt_Data, aligned_len);
        
        memcpy(&sSendBuffer[u16Send_Cnt], Encrypt_Data, aligned_len);
        u16Send_Cnt += aligned_len;
        u16Data_Cnt = aligned_len;
    } else {
        u16Data_Cnt = 0;
    }

    // Fill length
    sSendBuffer[4] = (u16Data_Cnt >> 8) & 0xFF;
    sSendBuffer[5] = u16Data_Cnt & 0xFF;
    
    // CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;

    // To AT Command
    snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n';

    Wireless_Uart_Send(su8_AT_Command,u16String_Cnt);
}

/*!
* \brief 无线模块发送握手包
* \param   none
* \return none
*/
void Wireless_Send_HandShake()
{
    uint8_t *MAC_ADRESS = Drv_GetBleMacAddress();
    Wireless_Send_Common(E_CMD_HANDSHAKE, MAC_ADRESS, 16);
}

/*!
* \brief 无线模块发送断开连接包
* \param   none
* \return none
*/
void Wireless_Send_Disconnect()
{
    Wireless_Send_Common(E_CMD_DISCONNECT, NULL, 0);
}

void Wirless_Reply_OTA_Request()
{
    uint8_t TxData[16];
    uint8_t SendCount = 0;
    BootLoader_TxData_Typedef *bootTxData = App_Bootloader_Get_TxData();

    TxData[SendCount++] = 0x01;
    TxData[SendCount++] = 0x01;
    TxData[SendCount++] = BleData_OTA_Tx.Ver[0];
    TxData[SendCount++] = BleData_OTA_Tx.Ver[1];
    TxData[SendCount++] = BleData_OTA_Tx.Ver[2];
    TxData[SendCount++] = BleData_OTA_Tx.Ble_ModuleMaxLen & 0xFF;
    TxData[SendCount++] = (BleData_OTA_Tx.Ble_ModuleMaxLen >> 8) & 0xFF;
    TxData[SendCount++] = BleData_OTA_Tx.OTA_PackMaxLen & 0xFF;
    TxData[SendCount++] = (BleData_OTA_Tx.OTA_PackMaxLen >> 8) & 0xFF; 
    Wireless_Send_Common(E_CMD_OTA_REQUEST, (uint8_t*)&TxData, SendCount);
}

void Wirless_Reply_OTA_FileInfo()
{
    uint8_t TxData[4];
    uint8_t SendCount = 0;

    TxData[SendCount++] = BleData_OTA_Tx.OTA_isAllowed ? 0x01 : 0x00;
    Wireless_Send_Common(E_CMD_OTA_FILE_INFO, (uint8_t*)&TxData, SendCount);
}

void Wireless_Reply_OTA_OffsetInfo()
{
    uint8_t TxData[4];
    uint8_t SendCount = 0;

    TxData[SendCount++] = (BleData_OTA_Tx.FileOffset) & 0xFF;
    TxData[SendCount++] = (BleData_OTA_Tx.FileOffset >> 8) & 0xFF;
    TxData[SendCount++] = (BleData_OTA_Tx.FileOffset >> 16) & 0xFF;
    TxData[SendCount++] = (BleData_OTA_Tx.FileOffset >> 24) & 0xFF;
    Wireless_Send_Common(E_CMD_OTA_OFFSET, (uint8_t*)&TxData, SendCount);
}


void Wireless_Reply_OTA_DataPacket()
{
    uint8_t TxData[4];
    uint8_t SendCount = 0;

    TxData[SendCount++] = BleData_OTA_Tx.PackDataStatus;
    Wireless_Send_Common(E_CMD_OTA_DATA, (uint8_t*)&TxData, SendCount);
}

void Wireless_Reply_OTA_UpdateResult()
{
    uint8_t TxData[4];
    uint8_t SendCount = 0;

    TxData[SendCount++] = BleData_OTA_Tx.UpdateResult;
    Wireless_Send_Common(E_CMD_OTA_RESULT, (uint8_t*)&TxData, SendCount);
}

/*!
* \brief 无线模块数据解包处理
* \param   cmd：命令字节
* \param   Recv_Data：接收数据缓冲区
* \return none
*/
void Data_Unpack_Handle(uint8_t cmd,uint8_t *Recv_Data)
{
    uint16_t CalOTACRC=0xFFFF;
    switch(cmd) {
        case E_CMD_HEARTBEAT:
            break;
        case E_CMD_DISCONNECT:
            Wireless_SendFlag.bits.Reply_Disconnect = 1;
            s_WireLessMgr.sendOffline = true;
            break;
        case E_CMD_OTA_REQUEST:
            // Handle OTA request
            if(Recv_Data[1] == 0xC8) {
                BleData_OTA_Rx.OTA_Req_BleMaxLen = true;  
            } 
            break;
        case E_CMD_OTA_FILE_INFO:
            // Handle OTA file info
            BleData_OTA_Rx.TargetVer[0] = Recv_Data[0];
            BleData_OTA_Rx.TargetVer[1] = Recv_Data[1];
            BleData_OTA_Rx.TargetVer[2] = Recv_Data[2];
            BleData_OTA_Rx.TargetVer[3] = Recv_Data[3];
            BleData_OTA_Rx.FileSize = (Recv_Data[4]) | (Recv_Data[5] << 8) | (Recv_Data[6] << 16) | (Recv_Data[7] << 24);
            BleData_OTA_Rx.FileCRC32 = (Recv_Data[8]) | (Recv_Data[9] << 8) | (Recv_Data[10] << 16) | (Recv_Data[11] << 24);
            BleData_OTA_Rx.OTA_Req_FileInfo = true;
            break;
        case E_CMD_OTA_OFFSET:
            // Handle OTA offset
            BleData_OTA_Rx.OTA_Offset = (Recv_Data[0]) | (Recv_Data[1] << 8) | (Recv_Data[2] << 16) | (Recv_Data[3] << 24);
            BleData_OTA_Rx.OTA_Req_OffsetInfo = true;
            break;
        case E_CMD_OTA_DATA:
            // Handle OTA data packet
            BleData_OTA_Tx.PackDataStatus = BL_PACK_S_ERR;
            BleData_OTA_Rx.OTA_PackSN  = (Recv_Data[0]) | (Recv_Data[1] << 8);
            BleData_OTA_Rx.OTA_DataLen = (Recv_Data[2]) | (Recv_Data[3] << 8);
            BleData_OTA_Rx.OTA_PackCRC = (Recv_Data[4]) | (Recv_Data[5] << 8);
            if(BleData_OTA_Rx.OTA_DataLen <= 128) {
                CalOTACRC = Crc16Compute(&Recv_Data[6], BleData_OTA_Rx.OTA_DataLen);
                if(CalOTACRC == BleData_OTA_Rx.OTA_PackCRC) {
                    BleData_OTA_Rx.OTA_Req_DataPacket = true;
                    memcpy(BleData_OTA_Rx.OTA_Data, &Recv_Data[6], BleData_OTA_Rx.OTA_DataLen);
                } else {
                    BleData_OTA_Tx.PackDataStatus = BL_PACK_S_CRC_ERR;
                    BleData_OTA_Rx.OTA_Req_DataPacketErr = true;
                }
            } else {
                BleData_OTA_Tx.PackDataStatus = BL_PACK_S_LEN_ERR;
                BleData_OTA_Rx.OTA_Req_DataPacketErr = true;
            }
            break;
        default:
            break;
    }
}

void Data_Send_Handle()
{
    static uint32_t lastTick = 0, currentTick = 0, SendDeathTime = 0,DeltaTick;
    BootLoader_TxData_Typedef *bootTxData = App_Bootloader_Get_TxData();
    DMA_TX_Status dma_status = Get_DMA_TX_Status(WirelessComm_Uart);
    if(SendDeathTime > 0) {
        currentTick = Drv_GetTick();
        DeltaTick = currentTick - lastTick;
        if(DeltaTick >= 10) {
            if(SendDeathTime <= DeltaTick) {
                SendDeathTime = 0;
            } else {
                SendDeathTime -= DeltaTick;
            }
            lastTick = currentTick;
        }
    }

    if(dma_status != DMA_TX_IDLE || SendDeathTime > 0) {
        return;
    }

    if(Wireless_SendFlag.bits.Reply_HandShake == 1) {
        Wireless_Send_HandShake();
        Wireless_SendFlag.bits.Reply_HandShake = 0;
        SendDeathTime = 50;
    }

    if(Wireless_SendFlag.bits.Reply_Disconnect == 1) {
        Wireless_SendFlag.bits.Reply_Disconnect = 0;
        Wireless_Send_Disconnect();
        SendDeathTime = 50;
    } 

    if(bootTxData->ReplyBleMaxLen == true) { 
        bootTxData->ReplyBleMaxLen = false;
        BleData_OTA_Tx.Ver[0] = FW_VER_MAJOR;
        BleData_OTA_Tx.Ver[1] = FW_VER_MINOR;
        BleData_OTA_Tx.Ver[2] = FW_VER_PATCH;
        BleData_OTA_Tx.Ble_ModuleMaxLen = BLE_MOUDLES_MAX_LEN;
        BleData_OTA_Tx.OTA_PackMaxLen = bootTxData->PacketMaxLen;
        Wirless_Reply_OTA_Request();
        SendDeathTime = 50;
    }

    if(bootTxData->OTA_Req_FileInfo == true) {
        bootTxData->OTA_Req_FileInfo = false;
        BleData_OTA_Tx.OTA_isAllowed = bootTxData->isAllowOTA;
        Wirless_Reply_OTA_FileInfo();
        SendDeathTime = 50;
    }

    if(bootTxData->OTA_Req_OffsetInfo == true) {
        bootTxData->OTA_Req_OffsetInfo = false;
        BleData_OTA_Tx.FileOffset = bootTxData->FileOffset;
        // Send OTA Offset Info if needed
        Wireless_Reply_OTA_OffsetInfo();
        SendDeathTime = 50;
    }

    if(bootTxData->OTA_Req_DataPacket == true) {
        bootTxData->OTA_Req_DataPacket = false;
        // Send OTA Data Packet Acknowledgment if needed
        BleData_OTA_Tx.PackDataStatus = bootTxData->PackStatus;
        Wireless_Reply_OTA_DataPacket();
        SendDeathTime = 50;
    }

    if(BleData_OTA_Rx.OTA_Req_DataPacketErr == true) {
        BleData_OTA_Rx.OTA_Req_DataPacketErr = false;
        // Send NACK for erroneous data packet
        Wireless_Reply_OTA_DataPacket();
        SendDeathTime = 50;
    }

    if(bootTxData->OTA_Req_UpdateResult == true) {
        bootTxData->OTA_Req_UpdateResult = false;
        BleData_OTA_Tx.UpdateResult = bootTxData->UpdateResult;
        Wireless_Reply_OTA_UpdateResult();
        SendDeathTime = 50;
    }
}

/*!
* \brief    无线模块处理
* \param   none
* \return none
*/
void WireLess_Process()
{
    static WireLess_Recv_EnumDef s_Wireless_Data_Recv_Flag;
    static uint16_t s_HandShake_Tick,s_Offline_Tick;
    static uint32_t lastTick, currentTick;
    switch(s_WireLessMgr.eState)
    {
        case BLE_HARD_INIT:
            App_Module_Reset();
            WirelessComm_Uart = Drv_BleModuleGetUart();
            Wireless_ChangeState(BLE_INIT_STATE);
            s_WireLessMgr.isInited = false;
            s_WireLessMgr.IsConnected = false;
            break;
        case BLE_INIT_STATE:
            // Handle initialization state
            if(!s_WireLessMgr.isInited) {
                if(App_Ble_Init() == 1) {
                    Init_MD5_key();
                    s_WireLessMgr.isInited = true;
                    s_WireLessMgr.IsConnected = false;
                    Wireless_ChangeState(BLE_DISCONNECT_STATE);
                    LOG_I("BLE Module Initialized\r\n");
                    LOG_I("Waiting for connection...\r\n");
                }
            }
            break;
        case BLE_DISCONNECT_STATE:
            s_Wireless_Data_Recv_Flag = (WireLess_Recv_EnumDef)Wireless_Handle_Data(&Ring_WireLessComm);
            if(s_Wireless_Data_Recv_Flag == wRecv_Puls) {
                s_Wireless_Data_Recv_Flag = wRecv_None;
                if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:CONNECTED") == 0) {
                    LOG_I("BLE Connected Start HandShake\r\n");
                    s_HandShake_Tick = 15000; // 15秒握手时间
                    lastTick = Drv_GetTick();
                      				
                }
				else if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:DISCONNECTED") == 0) {
                    LOG_I("BLE Disconnected\r\n");
                    s_HandShake_Tick = 0;
                }
            }
            else if (s_Wireless_Data_Recv_Flag == wRecv_Data){
                s_Wireless_Data_Recv_Flag = wRecv_None;
                uint8_t* MAC_ADRESS = Drv_GetBleMacAddress();
                // 解密数据，检查握手
                memcpy(Encrypt_Data,Ring_WireLessComm.RevData+6,Ring_WireLessComm.DataLen);
				my_aes_decrypt(Encrypt_Data,Decrypt_Data,Ring_WireLessComm.DataLen);
				if(strcmp((const char*)Decrypt_Data, (const char*)MAC_ADRESS) == 0) {
					s_WireLessMgr.IsConnected = true;
                    LOG_I("BLE HandShake OK\r\n");
				}
            }

            if(s_HandShake_Tick > 0) {
                currentTick = Drv_GetTick();
                if(currentTick - lastTick >= 10) {
                    s_HandShake_Tick -= (currentTick - lastTick);
                    lastTick = currentTick;
                    Wireless_Send_HandShake();  
                }
				if(s_WireLessMgr.IsConnected == true) {
                    LOG_I("BLE HandShake Sent, Time left: %d ms\r\n", s_HandShake_Tick);
                    Wireless_ChangeState(BLE_CONNECTED_STATE);
                    s_WireLessMgr.sendOffline = false;
                    s_Offline_Tick = 0;
                    s_HandShake_Tick = 0;
                    LOG_I("BLE Connected\r\n");
                    break;
				}

				if(s_HandShake_Tick <= 10) {
                    s_WireLessMgr.needDisconnect = true;
					s_HandShake_Tick = 0;
                    Drv_Delay(300);
				}
            }
            break;
        case BLE_CONNECTED_STATE:
            // Handle connected state 
            s_Wireless_Data_Recv_Flag = (WireLess_Recv_EnumDef)Wireless_Handle_Data(&Ring_WireLessComm);
            if(s_Wireless_Data_Recv_Flag == wRecv_Data) {
                s_Wireless_Data_Recv_Flag = wRecv_None;
                memcpy(Encrypt_Data,Ring_WireLessComm.RevData+6,Ring_WireLessComm.DataLen);
                my_aes_decrypt(Encrypt_Data,Decrypt_Data,Ring_WireLessComm.DataLen);
				Data_Unpack_Handle(Ring_WireLessComm.RevData[3],Decrypt_Data);
            }
            else if(s_Wireless_Data_Recv_Flag == wRecv_Puls){
                s_Wireless_Data_Recv_Flag = wRecv_None;
                if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:CONNECTED") == 0) {
                    s_WireLessMgr.sendOffline = false;
                    s_Offline_Tick = 0;                   
                }
				else if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:DISCONNECTED") == 0) {
                    LOG_I("BLE Disconnected\r\n");
                    s_WireLessMgr.sendOffline = true;
                    s_WireLessMgr.IsConnected = false;
                    s_Offline_Tick = 0;
                    lastTick = Drv_GetTick();
                }              
            }
			else if(s_Wireless_Data_Recv_Flag == wRecv_Char) {
                // if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") == 0) {
                //     Send_Rev_Ok_Flag = 1;                
                // }				
                // else if(strcmp((const char*)Ring_WireLessComm.RevData, "ERROR") == 0) {
                //     Send_Rev_Ok_Flag = 2;
                // }
			}
			if(s_WireLessMgr.sendOffline == true) {
                currentTick = Drv_GetTick();
                if(currentTick - lastTick >= 10) {
                    s_Offline_Tick += (currentTick - lastTick);
                    lastTick = currentTick;
                }
				if(s_Offline_Tick >= 1000) {  // 3s
					s_Offline_Tick = 0;
					s_WireLessMgr.sendOffline = false;
                    s_WireLessMgr.needDisconnect = true;
				}
			}
            Data_Send_Handle();
            break;
        case BLE_IDLE_STATE:
            // Handle idle state
            break;
        case BLE_STATE_MAX:
        default:
            // Handle unexpected state
            break;
    }
    if(s_WireLessMgr.needDisconnect == true) {
        s_WireLessMgr.needDisconnect = false;
        char cmd[] = "AT+QBLEDISCONN\r\n";
		Wireless_Uart_Send((uint8_t *)cmd, strlen(cmd));
        Wireless_ChangeState(BLE_HARD_INIT);
        LOG_I("BLE Disconnected Upload Data\r\n");
        Drv_Delay(300);
    }
    
}

bool App_Wireless_IsConnected(void)
{
    return s_WireLessMgr.IsConnected;
}

Wireless_OTA_RxData_Typedef *App_Wireless_Get_OTA_RxData(void)
{
    return &BleData_OTA_Rx;
}

/**************************End of file********************************/
