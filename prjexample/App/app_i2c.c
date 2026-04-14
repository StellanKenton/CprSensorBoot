/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_i2c.c
* \brief Implementation of I2C slave with enhanced error handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_i2c.h"
#include "main.h"
#include "lib_ringbuffer.h"
#include "app_feedback.h"
#include "app_power.h"
#include "app_Wireless.h"
#include "encryption.h"
#include "log.h"
/* Private macros -----------------------------------------------------------*/
#define I2C_RX_BUFFER_SIZE        16
#define I2C_TX_BUFFER_SIZE        16
#define I2C_MAX_RETRIES           3
#define I2C_TIMEOUT_MS            50

/* Private variables --------------------------------------------------------*/
I2C_Slave_TypeDef SwI2CDev = {
    .i2cDev = &hi2c2,
    .slaveAddr = SW_SLAVE_ADDR,
    .rxBuffer = {0},
    .txBuffer = {0},
    .errorStatus = {0},
    .retryCount = 0
};

static I2C_Connection_TypeDef g_IIC_Status;
static I2C_EncryData_TypeDef g_IIC_EncryData;

extern CBuff Ring_CPR_Press_Data;
extern PowerDown_State_StructDef Power_State;
extern Factory_TypeDef Factory_Info;
/* Private function prototypes ----------------------------------------------*/
static void I2C_LogError(I2C_ErrorCode errorCode);

/* Public functions ---------------------------------------------------------*/

/**
  * @brief Initialize I2C slave peripheral
  * @retval HAL status
  */
HAL_StatusTypeDef SlaveI2C_Init(void)
{
    HAL_StatusTypeDef status = HAL_OK;
    
    HAL_I2C_MspInit(SwI2CDev.i2cDev);
    
    // Clear error status
    SwI2CDev.retryCount = 0;
    
    status = HAL_I2C_EnableListen_IT(SwI2CDev.i2cDev);
    if (status != HAL_OK) {
        I2C_LogError(I2C_ERROR_INIT);
    }
    
    return status;
}

/**
  * @brief Listen complete callback
  * @param hi2c: I2C handle pointer
  */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == SwI2CDev.i2cDev) {
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

/**
  * @brief Address match callback
  * @param hi2c: I2C handle pointer
  * @param TransferDirection: Master request direction
  * @param AddrMatchCode: Matched address
  */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, 
                          uint8_t TransferDirection, 
                          uint16_t AddrMatchCode)
{
    uint8_t* mac;
    if (hi2c != SwI2CDev.i2cDev) return;  
    if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
        // Master wants to write to slave
        HAL_I2C_Slave_Seq_Receive_IT(hi2c, SwI2CDev.rxBuffer, 1, I2C_FIRST_FRAME);
        g_IIC_Status.Ticks = 5000;
        g_IIC_Status.connected = true;
    } else {
        // Master wants to read from slave
        uint8_t command = SwI2CDev.rxBuffer[0];
        uint8_t data_size = 0;
        
        switch(command) {
            case CMD_GET_DEVICE_ID:
                SwI2CDev.txBuffer[0] = SW_I2C_ID;
                data_size = 1;
                break;
                
            case CMD_QUERY_READABLE_DATA:
                SwI2CDev.txBuffer[0] = (uint8_t)(CBuff_GetLength(&Ring_CPR_Press_Data) >> 4);
                data_size = 1;
                break;         
            case CMD_GET_MAC_ADDRESS:
                mac = WirelessGetBleMacAdress();
                for(int i = 0; i < 6; i++) {
                    SwI2CDev.txBuffer[i] = mac[i];
                }
                data_size = 6;
                break;
                
            case CMD_GET_SERIAL_NUMBER:
                memcpy(SwI2CDev.txBuffer, g_IIC_EncryData.SNnumber, 16);
                data_size = 16;
                break;             
            case CMD_GET_PRESS_DATA:
                if (CBuff_GetLength(&Ring_CPR_Press_Data) >= 16) {
                    CBuff_Pop(&Ring_CPR_Press_Data, SwI2CDev.txBuffer, 16);
                    data_size = 16;
                } else {
                    I2C_LogError(I2C_ERROR_BUFFER_UNDERFLOW);
                    memset(SwI2CDev.txBuffer, 0xEE, 16); // Error pattern
                    data_size = 16;
                }
                break;
            case CMD_GET_SELF_TEST_RESULT:
                memcpy(SwI2CDev.txBuffer, g_IIC_EncryData.SelfCheck, 16);
                data_size = 16;
                break;
            case CMD_GET_DEVICE_INFO:
                memcpy(SwI2CDev.txBuffer, g_IIC_EncryData.DevInfo, 16);
                data_size = 16;
                break;
            default:
                I2C_LogError(I2C_ERROR_INVALID_CMD);
                SwI2CDev.txBuffer[0] = 0xFF; // Invalid command response
                data_size = 1;
                break;
        }
        
        HAL_I2C_Slave_Seq_Transmit_IT(hi2c, SwI2CDev.txBuffer, data_size, I2C_FIRST_FRAME);
    }
}

/**
  * @brief Slave receive complete callback
  * @param hi2c: I2C handle pointer
  */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == SwI2CDev.i2cDev) {
        // Reset retry counter on successful reception
        SwI2CDev.retryCount = 0;
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

/**
  * @brief Slave transmit complete callback
  * @param hi2c: I2C handle pointer
  */
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == SwI2CDev.i2cDev) {
        // Reset retry counter on successful transmission
        SwI2CDev.retryCount = 0;
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

/**
  * @brief I2C error callback
  * @param hi2c: I2C handle pointer
  */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != SwI2CDev.i2cDev) return;
    
    uint32_t halError = HAL_I2C_GetError(hi2c);
    
    // Map HAL errors to our error codes
    if (halError & HAL_I2C_ERROR_BERR) {
        I2C_LogError(I2C_ERROR_BUS);
    }
    if (halError & HAL_I2C_ERROR_ARLO) {
        I2C_LogError(I2C_ERROR_COMM_ERROR);
    }
    if (halError & HAL_I2C_ERROR_AF) {
        I2C_LogError(I2C_ERROR_ACK);
    }
    if (halError & HAL_I2C_ERROR_OVR) {
        I2C_LogError(I2C_ERROR_COMM_ERROR);
    }
    if (halError & HAL_I2C_ERROR_DMA) {
        I2C_LogError(I2C_ERROR_COMM_ERROR);
    }
    if (halError & HAL_I2C_ERROR_TIMEOUT) {
        I2C_LogError(I2C_ERROR_TIMEOUT);
    }
    
    // Handle retry mechanism
	I2C_LogError(I2C_ERROR_COMM_FAULT);	
	// Perform hard reset of I2C peripheral
	HAL_I2C_DeInit(hi2c);
	HAL_I2C_Init(hi2c);
	SlaveI2C_Init();
}

/* Private functions --------------------------------------------------------*/

/**
  * @brief Log I2C error code
  * @param errorCode: Detected error code
  */
static void I2C_LogError(I2C_ErrorCode errorCode)
{
    // Set the corresponding bit in the error status
    if (errorCode < I2C_ERROR_COUNT) {
        SwI2CDev.errorStatus.all |= (1UL << errorCode);
    }
    
    // Optional: Add error logging mechanism here
    // For example: LogToFlash(errorCode);
}

/**
  * @brief Get current I2C connection status
  * @retval Connection status (1 = connected, 0 = disconnected)
  */
bool I2C_GetConnectionStatus(void)
{
    return g_IIC_Status.connected;
}



/**
  * @brief Handle I2C connection status timeout
  */
void I2C_ConnectionStatusHandle(void)
{
    static uint16_t AutoPowerOff_Tick = 0;
    if(g_IIC_Status.connected) {
        g_IIC_Status.Ticks-=500;
        if(g_IIC_Status.Ticks == 0){
            AutoPowerOff_Tick = 10000;
            g_IIC_Status.connected = false;
        }
    }
    
    if(AutoPowerOff_Tick > 0){
        AutoPowerOff_Tick -= 500;
        if(CPR_is_Idle() == 0) {
			g_IIC_Status.Ticks = 10000;	
		}
        if(AutoPowerOff_Tick == 0)
		{
            LOG_I("  I2C Auto Power Down\r\n");
            App_Power_ShutDown();
			osDelay(1000);
        }
    }
}

void I2C_DataProcess(void)
{
    static bool DataNeedEncrypt = true;
    uint8_t su8_DataBuffer[16] = {0};
    uint8_t u16Data_Cnt = 0;
    SelfCheck_Typedef* selfTestData;
    BLE_FSM_EnumDef* BleState = Wireless_Get_State();
    if(*BleState == BLE_DISCONNECT_STATE || *BleState == BLE_IDLE_STATE) {
        // Connection is active, process data if needed
        if(DataNeedEncrypt) {
            // Perform data encryption here
            // Encrypt Serial Number
            memset(su8_DataBuffer, 0, 16);
            for(int i = 0; i < 13; i++) {
                su8_DataBuffer[i] = Factory_Info.Device_SN[i];
            }
            my_aes_encrypt(su8_DataBuffer, g_IIC_EncryData.SNnumber, 16);
            memset(su8_DataBuffer, 0, 16);

            // Encrypt Self Check Data
            selfTestData = GetSelfTestData();
            su8_DataBuffer[u16Data_Cnt++] = selfTestData->FeedBack_Self_Check ;
            su8_DataBuffer[u16Data_Cnt++] = selfTestData->Power_Self_Check;
            su8_DataBuffer[u16Data_Cnt++] = selfTestData->Audio_Self_Check;
            su8_DataBuffer[u16Data_Cnt++] = selfTestData->WirelessModlue_Self_Check;
            su8_DataBuffer[u16Data_Cnt++] = selfTestData->Memory_Self_Check;
            my_aes_encrypt(su8_DataBuffer, g_IIC_EncryData.SelfCheck, 16);
            memset(su8_DataBuffer, 0, 16);
            u16Data_Cnt = 0;

            // Encrypt Device Info Data
            su8_DataBuffer[u16Data_Cnt++] = (Factory_Info.Type >> 8);
            su8_DataBuffer[u16Data_Cnt++] = SoftWare_Version;
            su8_DataBuffer[u16Data_Cnt++] = SoftSub_Version;
            su8_DataBuffer[u16Data_Cnt++] = SoftBuild_Version;
            my_aes_encrypt(su8_DataBuffer, g_IIC_EncryData.DevInfo, 16);
            memset(su8_DataBuffer, 0, 16);
            u16Data_Cnt = 0;

            DataNeedEncrypt = false;
        }
    } else {
        // Connection is inactive, handle accordingly
        DataNeedEncrypt = true;
    }
}


void I2C_Loop_Process(void)
{
    //I2C_ConnectionStatusHandle();
    I2C_DataProcess();
}
/**************************End of file********************************/
