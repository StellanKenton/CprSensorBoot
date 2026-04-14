/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_memory.c
* \brief Implementation of memory management functions.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_memory.h"
#include "app_flash.h"
#include "semphr.h"
#include "app_wireless.h"
#include "app_system.h"
#include "log.h"
// 全局变量
Factory_TypeDef Factory_Info;
uint8_t Mem_Clear_Flag = 0;
uint8_t Wifi_Save_Flag = 0;
static char Default_UTC_Time[8] = "+08:00"; 
    
volatile Memory_SendState_TypeDef Memory_SendState;

static Memory_Block_Factory_TypeDef Dev_Factory_Info;
static volatile uint8_t read_buffer[128];
static volatile uint8_t Mem_Write_Buffer[64];
static CPRFeedbck_Data_Typedef Comm_Cpr_Data;
static uint32_t tmp_Container;
static Memory_Send_History_EnumDef History_Data_Send_State = History_Data_Send_Idle;
static Memory_Send_History_EnumDef History_Log_Send_State = History_Data_Send_Idle;
static Audio_Language_EnumDef Audio_Language_Mem = AUDIO_Default;
static uint8_t Audio_Volume_Mem = 0xFF;
static CPRFeedbck_time_Typedef CPR_TimeData;
static uint8_t CPR_Metronome_Mem;
static uint64_t UTC_Offset_Time_Mem;
static SingleStorageData_TypeDef SingleStorageData;

// 外部变量声明
extern osMessageQueueId_t CPR_WriteHandle;
extern osMessageQueueId_t CPR_ReadHandle;
extern osMessageQueueId_t Flash_WriteHandle;
extern osMessageQueueId_t Flash_ReadHandle;
extern osMessageQueueId_t CPR_Data_SaveHandle;
extern osMessageQueueId_t CPR_Time_SaveHandle;
extern osMessageQueueId_t CPR_Time_SendHandle;
extern osMessageQueueId_t Boot_Time_SendHandleHandle;
extern osMessageQueueId_t History_StatusHandle;
extern osMessageQueueId_t CPR_Data_SendHandle;
extern Audio_Language_EnumDef Audio_Language_Rev;
extern osMessageQueueId_t Log_SendHandle;
extern osMessageQueueId_t Log_SaveHandle;
extern osMessageQueueId_t Self_Check_SaveHandle;
extern osMessageQueueId_t Self_Check_SendHandle;
extern uint8_t Audio_Volume_Rev;
extern osSemaphoreId_t SaveSecretSemHandle;
extern char Device_Secret[18];
extern char productId[18];
extern uint8_t CPR_Metronome_Freq_Recv;
extern Wifi_Module_Typedef Wifi_Module;
extern PowerDown_State_StructDef Power_State;
extern char UTC_Offset_Time[6];
extern uint64_t UTC_Offset_Time_Trans;
// 单区块存储实例
SingleSectorStorage FactoryStorage = {
    .sector = MEM_FACTORY_START,
    .data_size = sizeof(Memory_Block_Factory_TypeDef),
    .data_ptr = &Dev_Factory_Info
};

SingleSectorStorage VolumeStorage = {
    .sector = MEM_VOLUME,
    .data_size = 8,
    .data_ptr = &SingleStorageData.Volume_Mem
};

SingleSectorStorage MetronomeStorage = {
    .sector = MEM_METRONOME,
    .data_size = 8,
    .data_ptr = &SingleStorageData.Metronome_Mem
};

SingleSectorStorage WifiStorage = {
    .sector = MEM_WIFI,
    .data_size = 64,
    .data_ptr = &SingleStorageData.Wifi_Mem
};

SingleSectorStorage SecretStorage = {
    .sector = MEM_SECRET,
    .data_size = 34,
    .data_ptr = &SingleStorageData.Secret_Mem
};

SingleSectorStorage UTCTimeStorage = {
    .sector = MEM_UTC,
    .data_size = 10,
    .data_ptr = &SingleStorageData.UTC_Offset_Time
};
// 环形缓冲区实例
RingBufferStorage CPRBuffer = {
    .start_sector = MEM_CPR_START,
    .end_sector = MEM_CPR_END,
    .sector_group_size = CPR_ACTIVE_SECTOR_NUM
};

RingBufferStorage LogBuffer = {
    .start_sector = MEM_LOG_START,
    .end_sector = MEM_LOG_END,
    .sector_group_size = 4  // 根据实际需求设置
};

/*!
* \brief 检查地址是否跨页
* \param   start_addr: 起始地址
*          length: 数据长度
* \return 1: 跨页 0: 不跨页
*/
uint8_t is_cross_page(uint32_t start_addr, uint16_t length) {
    uint32_t page_mask = 256 - 1;
    return ((start_addr & ~page_mask) != ((start_addr + length - 1) & ~page_mask));
}

/*!
* \brief 获取跨页的两个段长度
* \param   start_addr: 起始地址
*          length: 数据长度
*          first_seg_len: 第一段长度指针
*          second_seg_len: 第二段长度指针
* \return none
*/
void get_cross_page_segments(uint32_t start_addr, uint16_t length, 
                           uint16_t *first_seg_len, uint16_t *second_seg_len) {
    uint32_t remaining_in_first_page = 256 - (start_addr & 255);
    *first_seg_len = (length <= remaining_in_first_page) ? length : remaining_in_first_page;
    *second_seg_len = length - *first_seg_len;
}

/*!
* \brief 写入数据到Flash，处理跨页情况
* \param   addr: 写入地址
*          txdata: 待写入数据指针
*          len: 数据长度
* \return none
*/
void App_Memory_WritePage(uint32_t addr, volatile uint8_t *txdata, uint16_t len) {
    uint16_t first_seg_len, second_seg_len;
    if(is_cross_page(addr, len) == 0) {
        Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr, (uint8_t *)txdata, len);
    } else {     
        get_cross_page_segments(addr, len, &first_seg_len, &second_seg_len);
        Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr, (uint8_t *)txdata, first_seg_len);
        osDelay(5);
        Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr + first_seg_len, (uint8_t *)(txdata + first_seg_len), second_seg_len);
    }
    osDelay(10);
}

/*!
* \brief 初始化单区块存储
* \param   storage: 存储结构指针
* \return true: 成功 false: 失败
*/
bool SingleSector_Init(SingleSectorStorage* storage) {
    if (storage == NULL || storage->data_ptr == NULL) return false;
    
    uint8_t retry_count = 0;
    bool crc_valid = false;
    uint16_t Calculated_CheckSum;
    
    do {
        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, (storage->sector << 12), 
                                (uint8_t *)storage->data_ptr, storage->data_size) != HAL_OK) {
            retry_count++;
            osDelay(10);
            continue;
        }
        
        Calculated_CheckSum = Crc16Compute((const uint8_t*)storage->data_ptr, storage->data_size - 2);
        uint16_t stored_crc = *((uint16_t*)((uint8_t*)storage->data_ptr + storage->data_size - 2));
        
        if (Calculated_CheckSum == stored_crc) {
            crc_valid = true;
            break;
        }
        
        retry_count++;
        osDelay(10);
    } while (retry_count < 10);
    
    return crc_valid;
}

/*!
* \brief 读取单区块数据
* \param   storage: 存储结构指针
* \return true: 成功 false: 失败
*/
bool SingleSector_Read(SingleSectorStorage* storage) {
    if (storage == NULL || storage->data_ptr == NULL) return false;
    
    for (uint8_t i = 0; i < 3; i++) {
        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, (storage->sector << 12),
                                  (uint8_t *)storage->data_ptr, storage->data_size) == HAL_OK) {
            // 验证CRC
            uint16_t Calculated_CheckSum = Crc16Compute((const uint8_t*)storage->data_ptr, storage->data_size - 2);
            uint16_t stored_crc = *((uint16_t*)((uint8_t*)storage->data_ptr + storage->data_size - 2));

            if (Calculated_CheckSum == stored_crc) {
                return true;
            }
        }
        osDelay(10);
    }
    return false;
}

/*!
* \brief 写入单区块数据
* \param   storage: 存储结构指针
* \return true: 成功 false: 失败
*/
bool SingleSector_Write(SingleSectorStorage* storage) {
    if (storage == NULL || storage->data_ptr == NULL) return false;
    
    // 计算并存储CRC
    uint16_t Checksum = Crc16Compute((const uint8_t*)storage->data_ptr, storage->data_size - 2);
    *((uint16_t*)((uint8_t*)storage->data_ptr + storage->data_size - 2)) = Checksum;
    
    // 擦除扇区
    Drv_GD25Q32_EraseSector(&GD25Q32_Dev, (storage->sector << 12));
    osDelay(200);
    
    // 写入数据
    App_Memory_WritePage((storage->sector << 12), (uint8_t *)storage->data_ptr, storage->data_size);
    return true;
}


/*!
* \brief 写入测试设备信息到Flash
* \param   none
* \return none
*/
void App_Memory_Write_Test_Dev()
{
    volatile uint16_t Checksum;
    Drv_GD25Q32_EraseSector(&GD25Q32_Dev,(MEMORY_DEV_INFO_SECTOR << 12));
    osDelay(200);
    Memory_Block_Factory_TypeDef factory_data = {
        .type = DEV_FACTORY_INFO,
        .Device_Type = TYPE_HC630_AB,
        .Device_SN = {'H', 'C', '6', '3', '0', '2', '5', '7', 'T', '0', '0', '0', '4'},
        .CheckSum = 0,
    };
    Checksum = Crc16Compute(((const uint8_t*)&factory_data), (sizeof(Memory_Block_Factory_TypeDef)-2)); 
    factory_data.CheckSum = Checksum;
    memcpy((uint8_t *)Mem_Write_Buffer, &factory_data, sizeof(Memory_Block_Factory_TypeDef));
    App_Memory_WritePage((MEMORY_DEV_INFO_SECTOR << 12) + 0, Mem_Write_Buffer, sizeof(Memory_Block_Factory_TypeDef)); // Write block 0 offset 0
}

/*!
* \brief 初始化环形缓冲区
* \param   buffer: 缓冲区结构指针
* \return true: 成功 false: 失败
*/
bool RingBuffer_Init(RingBufferStorage* buffer) {
    if (buffer == NULL) return false;
    
    uint8_t zerosCount = 0;
    uint8_t find_Flag = 0;
    uint16_t Start_Offset = 0;
    uint16_t Find_Cnt = (buffer->end_sector - buffer->start_sector + 1) / buffer->sector_group_size;
    uint32_t tmp_Address;
    uint64_t Sector_Zero_Count;
    uint8_t tmp_Mem_Buf[16];
    uint16_t Calculated_CheckSum;

    buffer->current_address = (buffer->start_sector << 12);
    
    for (uint8_t i = 0; i < Find_Cnt; i++) {
        tmp_Address = ((buffer->start_sector + (i * buffer->sector_group_size)) << 12);
        osDelay(200);
        
        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, tmp_Address, 
                                (uint8_t *)&buffer->current_sector_info, sizeof(Memory_Block_Sector_TypeDef)) == HAL_OK) {
            Calculated_CheckSum = Crc16Compute(((const uint8_t*)&buffer->current_sector_info), 8); 
            if (buffer->current_sector_info.CheckSum == Calculated_CheckSum) {
                if (buffer->current_sector_info.Area_Status != AREA_DIRTY) {
                    for (uint8_t j = 0; j < buffer->sector_group_size; j++) {
                        tmp_Address = ((buffer->start_sector + (i * buffer->sector_group_size + j)) << 12);
                        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, tmp_Address, 
                                                (uint8_t *)&buffer->current_sector_info, sizeof(Memory_Block_Sector_TypeDef)) == HAL_OK) {
                            Calculated_CheckSum = Crc16Compute(((const uint8_t*)&buffer->current_sector_info), 8);
                            if (buffer->current_sector_info.CheckSum == Calculated_CheckSum) {
                                if (buffer->current_sector_info.Area_Status == AREA_ACTIVE) {
                                    Sector_Zero_Count = buffer->current_sector_info.cell_Contain_Status;
                                    for (uint32_t m = 0; m < 32; m++) {
                                        if ((Sector_Zero_Count & 1) == 0) {
                                            zerosCount++;
                                            Sector_Zero_Count >>= 1;
                                        } else {
                                            break;
                                        }
                                    }
                                    
                                    if (zerosCount > 0) {
                                        Start_Offset = ((zerosCount * 128 - sizeof(Memory_Block_Sector_TypeDef)) / DATA_BLOCK_SIZE) * DATA_BLOCK_SIZE + sizeof(Memory_Block_Sector_TypeDef);
                                    } else {
                                        Start_Offset = sizeof(Memory_Block_Sector_TypeDef);
                                    }
                                    
                                    buffer->current_address = ((buffer->start_sector + i * buffer->sector_group_size + j) << 12) + Start_Offset;
                                    
                                    for (uint32_t n = 0; n < 256; n++) {
                                        tmp_Address = (buffer->current_address + DATA_BLOCK_SIZE * n);
                                        find_Flag = 0;
                                        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, tmp_Address, tmp_Mem_Buf, 16) == HAL_OK) {
                                            for (int m = 0; m < 16; m++) {
                                                if (tmp_Mem_Buf[m] != DATA_EMPTY) {
                                                    find_Flag = 1;
                                                    break;
                                                }
                                            }
                                            if (find_Flag == 0) {
                                                buffer->current_address = tmp_Address;
                                                return true;
                                            }                                            
                                        } else {
                                            buffer->current_sector_info.type = DATA_ERR;
                                            buffer->current_sector_info.CheckSum = DATA_ERR;
                                            // 报告错误
                                        }
                                    }
                                    // 报告错误：所有单元都已使用
                                    return false;
                                } else {
                                    continue;
                                }
                            } else {
                                buffer->current_sector_info.type = DATA_ERR;
                                buffer->current_sector_info.CheckSum = DATA_ERR;
                                // 报告错误
                            }
                        } else {
                            buffer->current_sector_info.type = DATA_ERR;
                            buffer->current_sector_info.CheckSum = DATA_ERR;
                            // 报告错误
                        }
                    }
                } else {
                    continue;
                }
            } else {
                Drv_GD25Q32_EraseSector(&GD25Q32_Dev, ((buffer->start_sector + i * buffer->sector_group_size) << 12));
                osDelay(200);
                buffer->current_sector_info.type = DEV_SECTOR_INFO;
                buffer->current_sector_info.Area_Status = AREA_ACTIVE;
                buffer->current_sector_info.erase_count = 0;
                buffer->current_sector_info.cell_Contain_Status = 0xFFFFFFFF;
                buffer->current_sector_info.CheckSum = Crc16Compute(((const uint8_t*)&buffer->current_sector_info), 8);
                App_Memory_WritePage(buffer->current_address, (uint8_t *)&buffer->current_sector_info, sizeof(Memory_Block_Sector_TypeDef));
                osDelay(5);
                buffer->current_address = (buffer->start_sector << 12) + sizeof(Memory_Block_Sector_TypeDef);
                return true;
            }
        } else {
            buffer->current_sector_info.type = DATA_ERR;
            buffer->current_sector_info.CheckSum = DATA_ERR;
            // 报告错误
        }
    }
    return false;
}

/*!
* \brief 写入数据到环形缓冲区
* \param   buffer: 缓冲区结构指针
*          data: 待写入数据指针
* \return true: 成功 false: 失败
*/
bool RingBuffer_Write(RingBufferStorage* buffer, void* data) {
    if (buffer == NULL || data == NULL) return false;
    
    uint32_t Sector_Addr = buffer->current_address & 0xFFFFF000;
    uint32_t ZeroNum;
    uint32_t Write_Dirty_Sector_Addr;
	
    // 检查是否需要切换扇区
    if ((buffer->current_address & 0x00000FFF) + DATA_BLOCK_SIZE >= MEMORY_PROG_SIZE) {
        buffer->current_sector_info.Area_Status = AREA_FREE;
        App_Memory_WritePage(Sector_Addr, (uint8_t *)&buffer->current_sector_info, sizeof(Memory_Block_Sector_TypeDef));

        Sector_Addr = Sector_Addr >> 12;
        Sector_Addr += 1;
        if (Sector_Addr >= buffer->end_sector) {
            Sector_Addr = buffer->start_sector;
        }

        if ((Sector_Addr - buffer->start_sector) % buffer->sector_group_size == 0) {
            if (Sector_Addr == buffer->start_sector) {
                Write_Dirty_Sector_Addr = buffer->end_sector - buffer->sector_group_size;
            } else {
                Write_Dirty_Sector_Addr = Sector_Addr - buffer->sector_group_size;
            }
            Write_Dirty_Sector_Addr = Write_Dirty_Sector_Addr << 12;
            buffer->current_sector_info.Area_Status = AREA_DIRTY;
            App_Memory_WritePage(Write_Dirty_Sector_Addr, (uint8_t *)&buffer->current_sector_info, sizeof(Memory_Block_Sector_TypeDef));
        }
        
        Sector_Addr = Sector_Addr << 12;
        Drv_GD25Q32_EraseSector(&GD25Q32_Dev, Sector_Addr);
        osDelay(200);
        buffer->current_sector_info.type = DEV_SECTOR_INFO;
        buffer->current_sector_info.Area_Status = AREA_ACTIVE;
        buffer->current_sector_info.erase_count = 0;
        buffer->current_sector_info.cell_Contain_Status = 0xFFFFFFFF;
        buffer->current_sector_info.CheckSum = Crc16Compute(((const uint8_t*)&buffer->current_sector_info), 8);
        App_Memory_WritePage(Sector_Addr, (uint8_t *)&buffer->current_sector_info, sizeof(Memory_Block_Sector_TypeDef));
        buffer->current_address = Sector_Addr + sizeof(Memory_Block_Sector_TypeDef);
    }
            
    ZeroNum = (buffer->current_address & 0x00000FFF) / 128;
    tmp_Container = 0xFFFFFFFF;
    tmp_Container = tmp_Container << ZeroNum;
    
    // 计算并存储校验和
    uint16_t checksum = Crc16Compute(data, 14);
    *((uint16_t*)((uint8_t*)data + 14)) = checksum;
    
    App_Memory_WritePage(buffer->current_address, (uint8_t *)data, DATA_BLOCK_SIZE);
    buffer->current_address += DATA_BLOCK_SIZE;
    
    if (tmp_Container != buffer->current_sector_info.cell_Contain_Status) {
        buffer->current_sector_info.cell_Contain_Status &= tmp_Container;
        App_Memory_WritePage(Sector_Addr, (uint8_t *)&buffer->current_sector_info, sizeof(Memory_Block_Sector_TypeDef));
    }
    
    return true;
}

/*!
* \brief 从环形缓冲区读取下一个数据块
* \param   buffer: 缓冲区结构指针
*          data: 数据存储指针
* \return true: 成功 false: 失败或结束
*/
bool RingBuffer_ReadNext(RingBufferStorage* buffer, void* data) {
    if (buffer == NULL || data == NULL) return false;
    
    uint8_t tmp_Mem_Buf[DATA_BLOCK_SIZE];
    uint16_t Calculated_CheckSum;
    
    // 重试3次读取
    for (int i = 0; i < 3; i++) {
        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, buffer->read_address, tmp_Mem_Buf, DATA_BLOCK_SIZE) == HAL_OK) {
            break;
        }
    }
    
    Calculated_CheckSum = Crc16Compute(tmp_Mem_Buf, 14);
    if (Calculated_CheckSum != (tmp_Mem_Buf[14] | (tmp_Mem_Buf[15] << 8))) {
		buffer->read_address += DATA_BLOCK_SIZE;
		if (buffer->read_address > (buffer->end_sector << 12)) {
			buffer->read_address = (buffer->start_sector << 12) + sizeof(Memory_Block_Sector_TypeDef);
		}
        return false;
    }
    
    memcpy(data, tmp_Mem_Buf, DATA_BLOCK_SIZE);
    
    // 更新读取地址
    buffer->read_address += DATA_BLOCK_SIZE;
    if (buffer->read_address > (buffer->end_sector << 12)) {
        buffer->read_address = (buffer->start_sector << 12) + sizeof(Memory_Block_Sector_TypeDef);
    }
    
    return true;
}

/*!
* \brief 清除环形缓冲区
* \param   buffer: 缓冲区结构指针
* \return true: 成功 false: 失败
*/
bool RingBuffer_Clear(RingBufferStorage* buffer) {
    if (buffer == NULL) return false;
    
    uint8_t Reset_Status[2] = {0x00};
    for (uint32_t i = buffer->start_sector; i <= buffer->end_sector; i++) {
        App_Memory_WritePage(i << 12, Reset_Status, 1);
    }
    
    // 重新初始化缓冲区
    return RingBuffer_Init(buffer);
}

void Memory_PutTimeStampData(Gen_Data_Typedef *data)
{
    uint8_t tmp_Buffer[16];
    uint8_t Index = 0;
    CPRFeedbck_time_Typedef* CPR_TimeData = (CPRFeedbck_time_Typedef*)data;
    Index++;
    tmp_Buffer[Index++] = E_CMD_TIME_SYCN;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp >> 24) & 0xFF;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp >> 16) & 0xFF;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp >> 8)  & 0xFF;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp) & 0xFF;
    tmp_Buffer[0] = Index-1;
    for(uint8_t i=0;i<Index;i++) {
        Memory_SendState.SendBuffer[Memory_SendState.Index++] = tmp_Buffer[i];
    }
}

void Memory_PutBootTimeData(Gen_Data_Typedef *data)
{
    uint8_t tmp_Buffer[16];
    uint8_t Index = 0;
    CPRFeedbck_time_Typedef* CPR_TimeData = (CPRFeedbck_time_Typedef*)data;
    Index++;
    tmp_Buffer[Index++] = E_CMD_BOOT_TIME;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp >> 24) & 0xFF;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp >> 16) & 0xFF;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp >> 8)  & 0xFF;
    tmp_Buffer[Index++] = (CPR_TimeData->TimeStamp) & 0xFF;
    tmp_Buffer[0] = Index-1;
    for(uint8_t i=0;i<Index;i++) {
        Memory_SendState.SendBuffer[Memory_SendState.Index++] = tmp_Buffer[i];
    }
}

void Memory_PutCPRData(Gen_Data_Typedef *data)
{
    uint8_t tmp_Buffer[16];
    uint8_t Index = 0;
    CPRFeedbck_Data_Typedef* CPR_Data = (CPRFeedbck_Data_Typedef*)data;
    Index++;
    tmp_Buffer[Index++] = E_CMD_CPR_DATA;
    tmp_Buffer[Index++] = CPR_Data->TimeStamp >> 24;
    tmp_Buffer[Index++] = CPR_Data->TimeStamp >> 16;
    tmp_Buffer[Index++] = CPR_Data->TimeStamp >> 8;
    tmp_Buffer[Index++] = CPR_Data->TimeStamp & 0x000000FF;
    tmp_Buffer[Index++] = CPR_Data->Freq >> 8;
    tmp_Buffer[Index++] = CPR_Data->Freq & 0xFF;
    tmp_Buffer[Index++] = CPR_Data->Depth;
    tmp_Buffer[Index++] = CPR_Data->RealseDepth;
    tmp_Buffer[Index++] = CPR_Data->Interval;
    tmp_Buffer[Index++] = CPR_Data->BootStamp >> 24;
    tmp_Buffer[Index++] = CPR_Data->BootStamp >> 16;
    tmp_Buffer[Index++] = CPR_Data->BootStamp >> 8;
    tmp_Buffer[Index++] = CPR_Data->BootStamp & 0x000000FF;


    tmp_Buffer[0] = Index-1;
    for(uint8_t i=0;i<Index;i++) {
        Memory_SendState.SendBuffer[Memory_SendState.Index++] = tmp_Buffer[i];
    }
}

void Memory_PutSelfCheckData(Gen_Data_Typedef *data)
{
    uint8_t tmp_Buffer[16];
    uint8_t Index = 0;
    SelfCheck_Typedef* SelfCheck_Data = (SelfCheck_Typedef*)data;
    Index++;

    tmp_Buffer[Index++] = E_CMD_SELF_CHECK;
    tmp_Buffer[Index++] = SelfCheck_Data->FeedBack_Self_Check;
    tmp_Buffer[Index++] = SelfCheck_Data->Power_Self_Check;
    tmp_Buffer[Index++] = SelfCheck_Data->Audio_Self_Check;
    tmp_Buffer[Index++] = SelfCheck_Data->WirelessModlue_Self_Check;
    tmp_Buffer[Index++] = SelfCheck_Data->Memory_Self_Check;
    tmp_Buffer[Index++] = (SelfCheck_Data->TimeStamp >> 24) & 0xFF; 
    tmp_Buffer[Index++] = (SelfCheck_Data->TimeStamp >> 16) & 0xFF;
    tmp_Buffer[Index++] = (SelfCheck_Data->TimeStamp >> 8) & 0xFF;
    tmp_Buffer[Index++] = (SelfCheck_Data->TimeStamp) & 0xFF;

    tmp_Buffer[0] = Index-1;
    for(uint8_t i=0;i<Index;i++) {
        Memory_SendState.SendBuffer[Memory_SendState.Index++] = tmp_Buffer[i];
    }
}

void Memory_PutLogData(Gen_Data_Typedef *data)
{
    uint8_t tmp_Buffer[64];
    uint8_t Index = 0;
    LogUpdate_Typedef* Log_Data = (LogUpdate_Typedef*)data;
    Index++;
    tmp_Buffer[Index++] = E_CMD_LOG_DATA;
    tmp_Buffer[Index++] = (uint8_t)(Log_Data->TimeStamp >> 24);
	tmp_Buffer[Index++] = (uint8_t)(Log_Data->TimeStamp >> 16);
	tmp_Buffer[Index++] = (uint8_t)(Log_Data->TimeStamp >> 8);
	tmp_Buffer[Index++] = (uint8_t)(Log_Data->TimeStamp);
    tmp_Buffer[Index++] = Log_Data->Charge_Status;
    tmp_Buffer[Index++] = Log_Data->DC_Voltage;
    tmp_Buffer[Index++] = Log_Data->BAT_Voltage;
    tmp_Buffer[Index++] = Log_Data->V5_Voltage;
    tmp_Buffer[Index++] = Log_Data->V33_Voltage;
    tmp_Buffer[Index++] = Log_Data->Ble_State;
    tmp_Buffer[Index++] = Log_Data->Wifi_State;
    tmp_Buffer[Index++] = Log_Data->CPR_State;

    tmp_Buffer[0] = Index-1;
    for(uint8_t i=0;i<Index;i++) {
        Memory_SendState.SendBuffer[Memory_SendState.Index++] = tmp_Buffer[i];
    }
}
/*!
* \brief 发送历史数据
* \param   none
* \return none
*/
void History_Data_Handle() {
	uint32_t History_Sector_Addr;
	volatile Memory_Block_Sector_TypeDef tmp_Sector_Info;
	uint16_t Calculated_CheckSum;
    static Gen_Data_Typedef Gen_data;
  
    switch(History_Data_Send_State) {
        case History_Data_Send_Idle:         
            if (Memory_SendState.Upload == UPLOAD_HISTORY) {
                LOG_I("  History data send start\r\n");
                History_Data_Send_State = History_Data_Send_Start;
                Memory_SendState.Status = 0x01;
                Memory_SendState.UpdateStatus = true;             
                History_Sector_Addr = CPRBuffer.current_address & 0xFFFFF000;
                History_Sector_Addr = History_Sector_Addr >> 12;
                History_Sector_Addr += 1;
                if(History_Sector_Addr >= MEM_CPR_END)
                {
                    LOG_I("  History data is full and goto start\r\n");
                    History_Sector_Addr = MEM_CPR_START;
                }
                History_Sector_Addr = History_Sector_Addr << 12;
                if(Drv_GD25Q32_ReadPage(&GD25Q32_Dev,History_Sector_Addr, (uint8_t *)&tmp_Sector_Info, sizeof(Memory_Block_Sector_TypeDef)) == HAL_OK)
                {
                    Calculated_CheckSum = Crc16Compute(((const uint8_t*)&tmp_Sector_Info), 8);
                    if((tmp_Sector_Info.type != DEV_SECTOR_INFO)||(tmp_Sector_Info.CheckSum != Calculated_CheckSum)) {
                        History_Sector_Addr = MEM_CPR_START << 12;                    
                    }
                    CPRBuffer.read_address = History_Sector_Addr+16;
                }
                else {
                    // Report Error
                }
                LOG_I("  History data read address: 0x%08X\r\n", CPRBuffer.read_address);
            }
            break;
            
        case History_Data_Send_Start:
            memset((uint8_t *)Memory_SendState.SendBuffer, 0, sizeof(Memory_SendState.SendBuffer));
            Memory_SendState.Index = 0;
            if (RingBuffer_ReadNext(&CPRBuffer, &Gen_data)) {
                switch (Gen_data.type) {
                    case TIMESTAMP_DATA:
                        Memory_PutTimeStampData(&Gen_data);
                        Memory_SendState.SendBusyFlag = true;
                        break;
                    case BOOT_TIMESTAMP:
                        Memory_PutBootTimeData(&Gen_data);
                        Memory_SendState.SendBusyFlag = true;
                        break;
                }
                History_Data_Send_State = History_Data_Sending;
            } else {
                History_Data_Send_State = History_Data_Send_End;
                LOG_I("  History data find head\r\n");
            }
			if(Memory_SendState.Upload != UPLOAD_HISTORY) {
                History_Data_Send_State = History_Data_Send_End;
                LOG_I("  History data end because of method change%d\r\n", Memory_SendState.Upload);
            }
            break;
            
        case History_Data_Sending:
            if(Memory_SendState.SendBusyFlag == true) {
                break;
            }
            memset((uint8_t *)Memory_SendState.SendBuffer, 0, sizeof(Memory_SendState.SendBuffer));
            Memory_SendState.Index = 0;
            for(uint8_t i=0;i<6;i++) {      
                if(CPRBuffer.read_address != CPRBuffer.current_address) {         
                    if (RingBuffer_ReadNext(&CPRBuffer, &Gen_data)) {
                        switch (Gen_data.type) {
                            case TIMESTAMP_DATA:
                                Memory_PutTimeStampData(&Gen_data);
                                break;
                            case BOOT_TIMESTAMP:
                                Memory_PutBootTimeData(&Gen_data);
                                break;
                            case CPR_DATA:
                                Memory_PutCPRData(&Gen_data);
                                break;
                            // case DEV_LOG:
                            //     osMessageQueuePut(Log_SendHandle, &Gen_data, 0, 0);
                            //     break;
                            // case SELF_CHECK:
                            //     osMessageQueuePut(Self_Check_SendHandle, &Gen_data, 0, 0);
                            //     break;
                        }
                    } 
                }
                else {
                    History_Data_Send_State = History_Data_Send_End;
                    LOG_I("  History data send end\r\n");
                    break;
                }
                if(Memory_SendState.Upload != UPLOAD_HISTORY) {
                    History_Data_Send_State = History_Data_Send_End;
                    LOG_I("  History data end because of method change%d\r\n", Memory_SendState.Upload);
                    break;
                }
            }
            Memory_SendState.SendBusyFlag = true;
            break;
            
        case History_Data_Send_End:
            LOG_I("  History end address: 0x%08X\r\n", CPRBuffer.read_address);
            Memory_SendState.Status = 0x02;
            Memory_SendState.UpdateStatus = true;   
            History_Data_Send_State = History_Data_Send_Idle;
            Memory_SendState.Upload = UPLOAD_NONE;
            break;
        default:
            break;
    }
}
/*!
* \brief 发送历史log数据
* \param   none
* \return none
*/
void History_Log_Handle() {
	uint32_t History_Sector_Addr;
	volatile Memory_Block_Sector_TypeDef tmp_Sector_Info;
	uint16_t Calculated_CheckSum;
    static Gen_Data_Typedef Gen_data;
  
    switch(History_Log_Send_State) {
        case History_Data_Send_Idle:         
            if (Memory_SendState.Upload == UPLOAD_LOG) {
                History_Log_Send_State = History_Data_Send_Start;
                LOG_I("  History log send start\r\n");
                Memory_SendState.Status = 0x01;
                Memory_SendState.UpdateStatus = true;             
                History_Sector_Addr = LogBuffer.current_address & 0xFFFFF000;
                History_Sector_Addr = History_Sector_Addr >> 12;
                History_Sector_Addr += 1;
                if(History_Sector_Addr >= (MEM_LOG_END)){
                    History_Sector_Addr = (MEM_LOG_START);
                }
                History_Sector_Addr = History_Sector_Addr << 12;
                if(Drv_GD25Q32_ReadPage(&GD25Q32_Dev,History_Sector_Addr, (uint8_t *)&tmp_Sector_Info, sizeof(Memory_Block_Sector_TypeDef)) == HAL_OK)
                {
                    Calculated_CheckSum = Crc16Compute(((const uint8_t*)&tmp_Sector_Info), 8);
                    if((tmp_Sector_Info.type != DEV_SECTOR_INFO)||(tmp_Sector_Info.CheckSum != Calculated_CheckSum)) {
                        History_Sector_Addr = (MEM_LOG_START) << 12;                    
                    }
                    LogBuffer.read_address = History_Sector_Addr+16;
                }
                else {
                    // Report Error
                }
                LOG_I("  History log read address: 0x%08X\r\n", LogBuffer.read_address);
            }
            break;
            
        case History_Data_Send_Start:
            memset((uint8_t *)Memory_SendState.SendBuffer, 0, sizeof(Memory_SendState.SendBuffer));
            Memory_SendState.Index = 0;
            if (RingBuffer_ReadNext(&LogBuffer, &Gen_data)) {
                switch (Gen_data.type) {
                    case SELF_CHECK:
                        Memory_PutSelfCheckData(&Gen_data);
                        Memory_SendState.SendBusyFlag = true;
                        break;
                    case DEV_LOG:
                        Memory_PutLogData(&Gen_data);
                        Memory_SendState.SendBusyFlag = true;
                        break;
                    case TIMESTAMP_DATA:
                        Memory_PutTimeStampData(&Gen_data);
                        break;
                    case BOOT_TIMESTAMP:
                        Memory_PutBootTimeData(&Gen_data);
                        break;
                }
                History_Log_Send_State = History_Data_Sending;
            } else {
                History_Log_Send_State = History_Data_Send_End;
                LOG_I("  History data send end\r\n");
            }
			if(Memory_SendState.Upload != UPLOAD_LOG) {
                History_Log_Send_State = History_Data_Send_End;
                LOG_I("  History data end because of method change%d\r\n", Memory_SendState.Upload);
                
            }
            break;
            
        case History_Data_Sending:
            if(Memory_SendState.SendBusyFlag == true) {
                break;
            }
            memset((uint8_t *)Memory_SendState.SendBuffer, 0, sizeof(Memory_SendState.SendBuffer));
            Memory_SendState.Index = 0;
            for(uint8_t i=0;i<4;i++) {      
                if(LogBuffer.read_address != LogBuffer.current_address) {         
                    if (RingBuffer_ReadNext(&LogBuffer, &Gen_data)) {
                        switch (Gen_data.type) {
                            case SELF_CHECK:
                                Memory_PutSelfCheckData(&Gen_data);
                            case DEV_LOG:
                                Memory_PutLogData(&Gen_data);
                            case TIMESTAMP_DATA:
                                Memory_PutTimeStampData(&Gen_data);
                                break;
                            case BOOT_TIMESTAMP:
                                Memory_PutBootTimeData(&Gen_data);
                                break;
                        }
                    }
                }
                else {
                    History_Log_Send_State = History_Data_Send_End;
                    LOG_I("  History log send end\r\n");
                    break;
                }
                if(Memory_SendState.Upload != UPLOAD_LOG) {
                    History_Log_Send_State = History_Data_Send_End;
                    LOG_I("  History log end because of method change%d\r\n", Memory_SendState.Upload);
                    break;
                }
            }
            Memory_SendState.SendBusyFlag = true;
            break;
            
        case History_Data_Send_End:
            LOG_I("  History end address: 0x%08X\r\n", LogBuffer.read_address);
            Memory_SendState.Status = 0x02;
            Memory_SendState.UpdateStatus = true;   
            History_Log_Send_State = History_Data_Send_Idle;
            Memory_SendState.Upload = UPLOAD_NONE;
            break;
        default:
            break;
    }
}
/*!
* \brief 处理Flash读写请求
* \param   none
* \return none
*/
void Memory_Flash_Handle() {
    Comm_Flash_Data_Typedef tmp_Rev_Data;
    CPRFeedbck_Data_Typedef CPR_FlashData;
    LogUpdate_Typedef Log_Data;
    SelfCheck_Typedef Self_Check_Data;
    
    // 处理CPR数据写入
    if (osMessageQueueGet(CPR_WriteHandle, &Comm_Cpr_Data, NULL, 0) == osOK) {
       // 处理逻辑
    } 
    
    // 处理通用Flash写入
    if (osMessageQueueGet(Flash_WriteHandle, &tmp_Rev_Data, NULL, 0) == osOK) {
        if (tmp_Rev_Data.Cmd == 1) {
            App_Memory_WritePage(tmp_Rev_Data.Addr, (uint8_t *)tmp_Rev_Data.Data, tmp_Rev_Data.length);
        } else {
            Drv_GD25Q32_ReadPage(&GD25Q32_Dev, tmp_Rev_Data.Addr, (uint8_t *)tmp_Rev_Data.Data, tmp_Rev_Data.length);
            osMessageQueuePut(Flash_ReadHandle, &tmp_Rev_Data, 0, 0);
        }
    }
    
    // 处理时间数据写入
    if (osMessageQueueGet(CPR_Time_SaveHandle, &CPR_TimeData, NULL, 0) == osOK) {
        RingBuffer_Write(&CPRBuffer, &CPR_TimeData);
        RingBuffer_Write(&LogBuffer, &CPR_TimeData);
    }
    
    // 处理CPR数据写入
    if (osMessageQueueGet(CPR_Data_SaveHandle, &CPR_FlashData, NULL, 0) == osOK) {
        RingBuffer_Write(&CPRBuffer, &CPR_FlashData);
    }
    
    // 处理日志写入
    if (osMessageQueueGet(Log_SaveHandle, &Log_Data, NULL, 0) == osOK) {
        RingBuffer_Write(&LogBuffer, &Log_Data);
    }
    
    // 处理自检数据写入
    if (osMessageQueueGet(Self_Check_SaveHandle, &Self_Check_Data, NULL, 0) == osOK) {
        RingBuffer_Write(&LogBuffer, &Self_Check_Data);
    }
}

/*!
* \brief 读取音量设置
* \param   none
* \return none
*/
void Memory_Read_VolumeSetting() {
    uint8_t *VolumeData;
    if (SingleSector_Read(&VolumeStorage)) {
		VolumeData = VolumeStorage.data_ptr;
        if (VolumeData[0] == VOLUME_DATA) {
            Audio_Language_Mem = (Audio_Language_EnumDef)VolumeData[1];
            Audio_Volume_Mem = VolumeData[2];
        }
        Audio_Language_Rev = Audio_Language_Mem;
        Audio_Volume_Rev = Audio_Volume_Mem;
        LOG_I("  Volume read from memory: Language=%d, Volume=%d", Audio_Language_Rev, Audio_Volume_Rev);
    } else {
        Audio_Language_Rev = AUDIO_ZH;
        Audio_Volume_Rev = 3;
    }
}

/*!
* \brief 保存音量设置
* \param   none
* \return none
*/
void Memory_Save_Volume() {
	uint8_t VolumeData[8];
    if ((Audio_Language_Mem != Audio_Language_Rev) || (Audio_Volume_Mem != Audio_Volume_Rev)) {
        VolumeData[0] = VOLUME_DATA;
        VolumeData[1] = Audio_Language_Rev;
        VolumeData[2] = Audio_Volume_Rev;
        VolumeStorage.data_ptr = VolumeData;
        SingleSector_Write(&VolumeStorage);
        Audio_Language_Mem = Audio_Language_Rev;
        Audio_Volume_Mem = Audio_Volume_Rev;
    }
}

/*!
* \brief 设置WiFi保存标志
* \param   none
* \return none
*/
void Set_WifiSave_Flag() {
    Wifi_Save_Flag = 1;
}

/*!
* \brief 开始清除存储
* \param   cmd: 清除命令
* \return none
*/
void Set_Memory_Clear(uint8_t cmd) {
    Mem_Clear_Flag = cmd;
}

/*!
* \brief 获取存储清除状态
* \param   none
* \return 清除状态
*/
uint8_t Get_Memory_Clear() {
    return Mem_Clear_Flag;
}

/*!
* \brief 清除存储数据
* \param   none
* \return none
*/
void Clear_Memory_Data() {
    if (Mem_Clear_Flag == 1) {
        RingBuffer_Clear(&CPRBuffer);
        RingBuffer_Clear(&LogBuffer);
        Mem_Clear_Flag = 2;
        LOG_I("  Mem Clear\r\n");
        
        CPR_TimeData.type = BOOT_TIMESTAMP;
        Time_IOControl(TIME_POWERUP, TIME_GET, &CPR_TimeData.TimeStamp);
        osMessageQueuePut(CPR_Time_SaveHandle, &CPR_TimeData, 0, 0);
    }
}

/*!
* \brief 保存设备密钥
* \param   none
* \return none
*/
void Memory_Save_Secret() {
	uint8_t SecretData[34] = {0};
    if (xSemaphoreTake(SaveSecretSemHandle, 0) == pdTRUE) {       
        memcpy(SecretData, Device_Secret, 16);
        memcpy(SecretData + 16, productId, 16);
        SecretStorage.data_ptr = SecretData;
        SingleSector_Write(&SecretStorage);
    }
}

/*!
* \brief 保存节拍器设置
* \param   none
* \return none
*/
void Memory_Save_Metronome() {
	uint8_t MetronomeData[8];
    if (CPR_Metronome_Mem != CPR_Metronome_Freq_Recv) {
        MetronomeData[0] = METRONOME_DATA;
		MetronomeData[1] = CPR_Metronome_Freq_Recv;
        MetronomeStorage.data_ptr = MetronomeData;
        SingleSector_Write(&MetronomeStorage);
        CPR_Metronome_Mem = CPR_Metronome_Freq_Recv;
    }
}

/*!
* \brief 读取节拍器设置
* \param   none
* \return none
*/
void Memory_Read_Metronome() {
    uint8_t *MetronomeData;
    if (SingleSector_Read(&MetronomeStorage)) {
        MetronomeData = MetronomeStorage.data_ptr;
        if (MetronomeData[0] == METRONOME_DATA) {
            CPR_Metronome_Mem = MetronomeData[1];
        }
        CPR_Metronome_Freq_Recv = CPR_Metronome_Mem;
        LOG_I("  Metronome read from memory: Freq=%d", CPR_Metronome_Freq_Recv);
    } else {
        CPR_Metronome_Freq_Recv = 110;
    }
}

/*!
* \brief 保存WiFi设置
* \param   none
* \return none
*/
void Memory_Save_WIFI() {
    uint8_t u8Data_Cnt = 0;
    uint8_t WifiData[64] = {0};
    if (Wifi_Save_Flag == 1) {
        Wifi_Save_Flag = 0;   
        WifiData[u8Data_Cnt++] = WIFI_DATA;
        WifiData[u8Data_Cnt++] = strlen((const char*)Wifi_Module.SSID);
        for (int i = 0; i < strlen((const char*)Wifi_Module.SSID); i++) {
            WifiData[u8Data_Cnt++] = Wifi_Module.SSID[i];
        }
        WifiData[u8Data_Cnt++] = strlen((const char*)Wifi_Module.PWD);
        for (int i = 0; i < strlen((const char*)Wifi_Module.PWD); i++) {
            WifiData[u8Data_Cnt++] = Wifi_Module.PWD[i];
        }
        WifiStorage.data_ptr = WifiData;
        SingleSector_Write(&WifiStorage);
    }
}

/*!
* \brief 读取WiFi设置
* \param   none
* \return none
*/
void Memory_Read_WIFI() {
    uint8_t *WifiData;
    if (SingleSector_Read(&WifiStorage)) {
        WifiData = WifiStorage.data_ptr;
        if (WifiData[0] == WIFI_DATA) {
            uint8_t ssid_len = WifiData[1];
            memcpy(Wifi_Module.SSID, &WifiData[2], ssid_len);
            Wifi_Module.SSID[ssid_len] = '\0';
            
            uint8_t pwd_start = 2 + ssid_len;
            uint8_t pwd_len = WifiData[pwd_start];
            memcpy(Wifi_Module.PWD, &WifiData[pwd_start + 1], pwd_len);
            Wifi_Module.PWD[pwd_len] = '\0';
        }
    } else {
        memset(Wifi_Module.SSID, 0, sizeof(Wifi_Module.SSID));
        memset(Wifi_Module.PWD, 0, sizeof(Wifi_Module.PWD));
    }
}

/*!
* \brief 读取设备密钥
* \param   none
* \return none
*/
void Memory_Read_Secret() {
    uint8_t *SecretData;
    if (xSemaphoreTake(SaveSecretSemHandle, 0) == pdTRUE) {
        SecretData = SecretStorage.data_ptr;
        if (SingleSector_Read(&SecretStorage)) {
            memcpy(Device_Secret, SecretData, 16);
            memcpy(productId, SecretData + 16, 16);
        } else {
            memset(Device_Secret, 0, 16);
            memset(productId, 0, 16);
        }
    }
}

/*!
* \brief 保存UTC时间偏移
* \param   none
* \return none
*/
void Memory_Save_UTC_Time() {
	static volatile uint8_t Utc_TimeData[7];
    if (UTC_Offset_Time_Mem != UTC_Offset_Time_Trans) {
        Utc_TimeData[0] = METRONOME_DATA;
        for(int i = 0; i < 6; i++) {
            Utc_TimeData[i + 1] = UTC_Offset_Time[i];
        }
        UTCTimeStorage.data_ptr = (uint8_t *)Utc_TimeData;
        SingleSector_Write(&UTCTimeStorage);
        UTC_Offset_Time_Mem = UTC_Offset_Time_Trans;
    }
}

/*!
* \brief 读取UTC时间偏移
* \param   none
* \return none
*/
void Memory_Read_UTC_Time() {
    uint8_t *Utc_Time;
    char Memory_UTC_Time[6] = {0};
    if (SingleSector_Read(&UTCTimeStorage)) {
        Utc_Time = UTCTimeStorage.data_ptr;
        if (Utc_Time[0] == METRONOME_DATA) {
            for (int i = 0; i < 6; i++) {
                Memory_UTC_Time[i] = Utc_Time[i + 1];
            }
        }
        memcpy(UTC_Offset_Time, Memory_UTC_Time, 6);
        memcpy(&UTC_Offset_Time_Trans, Memory_UTC_Time, 6);
        memcpy(&UTC_Offset_Time_Mem, Memory_UTC_Time, 6);
        LOG_I("  UTC time read from memory: %s\r\n", UTC_Offset_Time);
    } else {
        memcpy(UTC_Offset_Time, Default_UTC_Time, 6);
        memcpy(&UTC_Offset_Time_Trans, Default_UTC_Time, 6);
        memcpy(&UTC_Offset_Time_Mem, Default_UTC_Time, 6);
    }
}

void App_Memory_ShutDownCheck()
{
    static uint16_t ShutDown_Check_Cnt = 0;
    if((Power_State.state == DEV_POWER_OFF)&&(Power_State.flag.bits.feedback == 0))
    {
        ShutDown_Check_Cnt += MEMORY_OS_DELAY;
        if(ShutDown_Check_Cnt >= 300)
        {
            ShutDown_Check_Cnt = 0;
            Power_State.flag.bits.memory = 0;
        }
    }
    else
    {
        ShutDown_Check_Cnt = 0;
    }
}

void Memory_RTTWrtieSNProcess(char *data)
{
    
}

/*!
* \brief 初始化存储模块
* \param   none
* \return none
*/
void App_Memory_init(void) {

    //App_Memory_Write_Test_Dev();
    Memory_SendState.Upload = UPLOAD_NONE;
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4,GPIO_PIN_SET);
    App_Flash_Init();
    // 初始化单区块存储
    if (SingleSector_Init(&FactoryStorage)) {
        Factory_Info.Type = (Device_Type_EnumDef)Dev_Factory_Info.Device_Type;
        memcpy(Factory_Info.Device_SN, Dev_Factory_Info.Device_SN, 13);
        
        switch(Factory_Info.Type) {
            case TYPE_HC600_N:      memcpy(Factory_Info.Name, "HC600", 6); break;
            case TYPE_HC610_A:      memcpy(Factory_Info.Name, "HC610", 6); break;
            case TYPE_HC620_B:      memcpy(Factory_Info.Name, "HC620", 6); break;
            case TYPE_HC630_AB:     memcpy(Factory_Info.Name, "HC630", 6); break;
            case TYPE_HCTEST_ABC:   memcpy(Factory_Info.Name, "HCtest", 6); break;
            default:                memcpy(Factory_Info.Name, "HC6XX", 6); break; 
        }      
        g_Err.Memory.bits.Self_Check_Ok = 1;
    } else {
        g_Err.Memory.bits.Init_Err = 1;
    }
    LOG_I("  Device Type: %s, SN: %s", Factory_Info.Name, Factory_Info.Device_SN);
    // 初始化环形缓冲区
    RingBuffer_Init(&CPRBuffer);
    RingBuffer_Init(&LogBuffer);
    
    // 读取其他设置
    Memory_Read_VolumeSetting();
    Memory_Read_Secret();
    Memory_Read_Metronome();
    Memory_Read_WIFI();
    Memory_Read_UTC_Time();
} 

/*!
* \brief 处理存储模块任务
* \param   none
* \return none
*/
void App_Memory_Handle() {
    App_Memory_ShutDownCheck();
    
    Memory_Flash_Handle();
    App_Flash_Handle();
    History_Data_Handle();  
    History_Log_Handle();
    Clear_Memory_Data();
    
    Memory_Save_Volume();
    Memory_Save_Secret();
    Memory_Save_Metronome();
    Memory_Save_WIFI();
    Memory_Save_UTC_Time();
}
/**************************End of file********************************/
