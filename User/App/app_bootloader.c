#include "app_bootloader.h"
#include "drv_mcu_flash.h"
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "app_wireless.h"
#include "drv_delay.h"

static BootloaderMgr_t s_BootLoaderMgr;
static BootLoader_TxData_Typedef s_BootLoader_TxData;


static uint32_t Bootloader_CRC32Update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void BootLoader_WriteBootFlag(uint8_t app_area,uint8_t status)
{
    Bootloader_BootFlag_t ReadBootFlag;
    Bootloader_BootFlag_t WriteBootFlag;
    Bootloader_BootFlag_t TargetBootFlag;
    bool WireBackUpFlag = false;
    uint32_t BootReadFlagCRC = 0;
    Drv_ExtFlash_Read(EXT_FLAG_START_ADDR, (uint8_t*)&ReadBootFlag, sizeof(ReadBootFlag));
    BootReadFlagCRC = Bootloader_CRC32Update(0xFFFFFFFF, (uint8_t*)&ReadBootFlag, sizeof(ReadBootFlag) - sizeof(ReadBootFlag.CRC_Data));
    if(BootReadFlagCRC != ReadBootFlag.CRC_Data || (ReadBootFlag.AppArea1 > FLASH_FLAG_UP || ReadBootFlag.AppArea2  > FLASH_FLAG_UP) ||
       (ReadBootFlag.AppArea1 < FLASH_FLAG_BLK || ReadBootFlag.AppArea2 < FLASH_FLAG_APP)) {
        LOG_E("Boot Flag CRC Mismatch or Invalid! Using Backup Boot Flag.\r\n");
        Drv_ExtFlash_Read(EXT_FLAG_BACKUP_ADDR, (uint8_t*)&ReadBootFlag, sizeof(ReadBootFlag));
        Drv_ExtFlash_Read(EXT_FLAG_BACKUP_ADDR+sizeof(ReadBootFlag), (uint8_t*)&TargetBootFlag, sizeof(TargetBootFlag));
        BootReadFlagCRC = Bootloader_CRC32Update(0xFFFFFFFF, (uint8_t*)&ReadBootFlag, sizeof(ReadBootFlag) - sizeof(ReadBootFlag.CRC_Data));
        if(BootReadFlagCRC != ReadBootFlag.CRC_Data) {
            LOG_E("Backup Boot Flag CRC Mismatch! Cannot update Boot Flag.\r\n");
            return;
        }
        LOG_I("Backup Boot Flag Valid. AppArea1: 0x%02X, AppArea2: 0x%02X\r\n", ReadBootFlag.AppArea1, ReadBootFlag.AppArea2);
        BootReadFlagCRC = Bootloader_CRC32Update(0xFFFFFFFF, (uint8_t*)&TargetBootFlag, sizeof(TargetBootFlag) - sizeof(TargetBootFlag.CRC_Data));
        if(BootReadFlagCRC != TargetBootFlag.CRC_Data) {
            LOG_E("Target Boot Flag CRC Mismatch! Cannot update Boot Flag.\r\n");
            return;
        }
        // to be done ,判断之前的状态是什么，以及现在要写入的状态是什么，再判断应该怎么做
    } else {
        WireBackUpFlag = true;
    }
    WriteBootFlag = ReadBootFlag;
    if(app_area == 1) {
        WriteBootFlag.AppArea1 = status;
    } else if(app_area == 2) {
        WriteBootFlag.AppArea2 = status;
    } else {
        LOG_E("Invalid App Area specified for Boot Flag write: %d\r\n", app_area);
        return;
    }
    WriteBootFlag.CRC_Data = Bootloader_CRC32Update(0xFFFFFFFF, (uint8_t*)&WriteBootFlag, sizeof(WriteBootFlag) - sizeof(WriteBootFlag.CRC_Data));
    if(WireBackUpFlag == true) {
        Drv_ExtFlash_ErasePage(EXT_FLAG_BACKUP_ADDR);
        Drv_Delay(100);
        Drv_ExtFlash_Write(EXT_FLAG_BACKUP_ADDR, (uint8_t*)&ReadBootFlag, sizeof(ReadBootFlag));
        Drv_Delay(10);
        Drv_ExtFlash_Write(EXT_FLAG_BACKUP_ADDR+sizeof(ReadBootFlag), (uint8_t*)&WriteBootFlag, sizeof(WriteBootFlag));
        Drv_Delay(10);
        LOG_I("Boot Flag backup written to external flash at backup location.\r\n");
    }
    Drv_ExtFlash_ErasePage(EXT_FLAG_START_ADDR);
    Drv_Delay(100);
    Drv_ExtFlash_Write(EXT_FLAG_START_ADDR, (uint8_t*)&WriteBootFlag, sizeof(WriteBootFlag));
    Drv_Delay(10);
}

/**
  * @brief  Initialize Bootloader
  */
void App_Bootloader_Init(void)
{
    // Any initialization required for the bootloader state
    Drv_McuFlash_Init();
    s_BootLoaderMgr.state = BOOT_STATE_INIT;
    s_BootLoaderMgr.NeedUpdate = false;
    LOG_I("Bootloader Init\r\n");
}

void App_BootLoader_JumpCheck(void)
{
    // Placeholder for any checks before jumping to app
    Bootloader_BootFlag_t ReadBootFlag;
    uint32_t BootReadFlagCRC = 0;
    Drv_ExtFlash_Read(EXT_FLAG_START_ADDR, (uint8_t*)&ReadBootFlag, sizeof(ReadBootFlag));
    BootReadFlagCRC = Bootloader_CRC32Update(0xFFFFFFFF, (uint8_t*)&ReadBootFlag, sizeof(ReadBootFlag) - sizeof(ReadBootFlag.CRC_Data));
    s_BootLoaderMgr.boot_flag = ReadBootFlag;
    if(BootReadFlagCRC != ReadBootFlag.CRC_Data) {
        LOG_E("Boot Flag CRC Mismatch! Invalid Boot Flag.\r\n");
        s_BootLoaderMgr.NeedUpdate = true;
    } else {
        if((ReadBootFlag.AppArea1 > FLASH_FLAG_UP || ReadBootFlag.AppArea2  > FLASH_FLAG_UP) ||
           (ReadBootFlag.AppArea1 < FLASH_FLAG_BLK || ReadBootFlag.AppArea2 < FLASH_FLAG_APP)) {
            LOG_E("Boot Flag Invalid Values! Resetting Boot Flag.\r\n");
            s_BootLoaderMgr.NeedUpdate = true;
        } else {
            LOG_I("Boot Flag Valid. AppArea1: 0x%02X, AppArea2: 0x%02X\r\n", ReadBootFlag.AppArea1, ReadBootFlag.AppArea2);
            if(ReadBootFlag.AppArea1 == FLASH_FLAG_APP || ReadBootFlag.AppArea2 == FLASH_FLAG_APP) {
                LOG_I("Valid Application Found. Jumping to Application...\r\n");
                App_Bootloader_JumpToApp(APP_START_ADDR);
            } else {
                LOG_I("No Valid Application. Staying in Bootloader for Update.\r\n");
                // to be done ,有效无效应该怎么做
                s_BootLoaderMgr.NeedUpdate = true;
            }
        }    
    } 
    
}

bool App_Bootloader_GetNeedUpdate(void)
{
    return s_BootLoaderMgr.NeedUpdate;
}

void Bootloader_ChangeState(BootloaderState_t newState)
{
    if(newState != s_BootLoaderMgr.state && newState < BOOT_STATE_ERROR){
        LOG_I("Bootloader State changed from %d to %d", s_BootLoaderMgr.state, newState);
        s_BootLoaderMgr.state = newState;
    }   
}

/**
  * @brief  Handle flash operation error
  */
static void Bootloader_Flash_HandleError(BootLoader_UpdateResult_e error_code, bool *pFlashInitialized, uint32_t *pMcuWriteOffset)
{
    Bootloader_ChangeState(BOOT_STATE_ERROR);
    s_BootLoader_TxData.OTA_Req_UpdateResult = true;
    s_BootLoader_TxData.UpdateResult = error_code;
    if (pFlashInitialized != NULL) {
        *pFlashInitialized = false;
    }
    if (pMcuWriteOffset != NULL) {
        *pMcuWriteOffset = 0xFFFFFFFF;
    }
}

/**
  * @brief  Determine CurrentAppArea based on FlashStartAddr
  * @retval true on success, false on failure
  */
static bool Bootloader_Flash_DetermineAppArea(void)
{
    if (s_BootLoaderMgr.CurrentAppArea == 0) {
        if (s_BootLoaderMgr.FlashStartAddr == EXT_APP1_START_ADDR) {
            s_BootLoaderMgr.CurrentAppArea = 1;
        } else if (s_BootLoaderMgr.FlashStartAddr == EXT_APP2_START_ADDR) {
            s_BootLoaderMgr.CurrentAppArea = 2;
        } else {
            LOG_E("Invalid FlashStartAddr: 0x%08X\r\n", s_BootLoaderMgr.FlashStartAddr);
            return false;
        }
    }
    return true;
}

/**
  * @brief  Validate file size does not exceed app area
  * @retval true if valid, false if exceeds limit
  */
static bool Bootloader_Flash_ValidateFileSize(void)
{
    uint32_t file_size = s_BootLoaderMgr.SavedFileSize;
    uint32_t app_end_addr = APP_START_ADDR + file_size;
    
    if (app_end_addr > APP_END_ADDR) {
        LOG_E("File size exceeds app area: %lu bytes\r\n", file_size);
        return false;
    }
    return true;
}

/**
  * @brief  Erase required pages in MCU flash app area
  * @retval true on success, false on failure
  */
static bool Bootloader_Flash_ErasePages(void)
{
    uint32_t file_size = s_BootLoaderMgr.SavedFileSize;
    uint32_t pages_to_erase = (file_size + MCU_FLASH_PAGE_SIZE - 1) / MCU_FLASH_PAGE_SIZE;
    
    LOG_I("Erasing %lu pages in MCU flash app area...\r\n", pages_to_erase);
    uint32_t erase_addr = APP_START_ADDR;
    
    for (uint32_t i = 0; i < pages_to_erase; i++) {
        if (Drv_McuFlash_ErasePage(erase_addr) != MCU_FLASH_OK) {
            LOG_E("Failed to erase page at 0x%08X\r\n", erase_addr);
            return false;
        }
        erase_addr += MCU_FLASH_PAGE_SIZE;
        Drv_Delay(10); // Small delay between erase operations
    }
    
    LOG_I("MCU flash pages erased successfully\r\n");
    return true;
}

/**
  * @brief  Copy one chunk of data from external flash to MCU flash
  * @param  pMcuWriteOffset: Pointer to current write offset, updated on success
  * @retval true on success, false on failure
  */
static bool Bootloader_Flash_CopyChunk(uint32_t *pMcuWriteOffset)
{
    #define FLASH_COPY_BUFFER_SIZE 512  // Buffer size for copying
    uint8_t copy_buffer[FLASH_COPY_BUFFER_SIZE];
    
    uint32_t ext_read_addr = s_BootLoaderMgr.FlashStartAddr + *pMcuWriteOffset;
    uint32_t mcu_write_addr = APP_START_ADDR + *pMcuWriteOffset;
    uint32_t remaining = s_BootLoaderMgr.SavedFileSize - *pMcuWriteOffset;
    uint32_t chunk_size = (remaining > FLASH_COPY_BUFFER_SIZE) ? FLASH_COPY_BUFFER_SIZE : remaining;
    
    if (chunk_size == 0) {
        return true; // Nothing to copy
    }
    
    // Read from external flash
    if (Drv_ExtFlash_Read(ext_read_addr, copy_buffer, 256) != 0) {
        LOG_E("Failed to read from external flash at 0x%08X\r\n", ext_read_addr);
        return false;
    }
    
    if (Drv_ExtFlash_Read(ext_read_addr+256, copy_buffer+256, 256) != 0) {
        LOG_E("Failed to read from external flash at 0x%08X\r\n", ext_read_addr);
        return false;
    }
    
    // Write to MCU flash
    if (Drv_McuFlash_Write(mcu_write_addr, copy_buffer, chunk_size) != MCU_FLASH_OK) {
        LOG_E("Failed to write to MCU flash at 0x%08X\r\n", mcu_write_addr);
        return false;
    }
    
    *pMcuWriteOffset += chunk_size;
    LOG_D("Copied %lu bytes, progress: %lu/%lu bytes\r\n", 
          chunk_size, *pMcuWriteOffset, s_BootLoaderMgr.SavedFileSize);
    
    return true;
}

/**
  * @brief  Verify written data by comparing CRC32
  * @retval true if verification passed, false if failed
  */
static bool Bootloader_Flash_VerifyData(void)
{
    #define FLASH_COPY_BUFFER_SIZE 512
    uint8_t verify_buffer[FLASH_COPY_BUFFER_SIZE];
    uint32_t verify_crc = 0;
    uint32_t verify_addr = APP_START_ADDR;
    uint32_t verify_remaining = s_BootLoaderMgr.SavedFileSize;
    
    LOG_I("Flash copy completed, verifying data...\r\n");
    
    while (verify_remaining > 0) {
        uint32_t verify_chunk = (verify_remaining > FLASH_COPY_BUFFER_SIZE) ? 
                               FLASH_COPY_BUFFER_SIZE : verify_remaining;
        
        // Read from MCU flash
        if (Drv_McuFlash_Read(verify_addr, verify_buffer, verify_chunk) != MCU_FLASH_OK) {
            LOG_E("Failed to read from MCU flash for verification at 0x%08X\r\n", verify_addr);
            return false;
        }
        
        verify_crc = Bootloader_CRC32Update(verify_crc, verify_buffer, verify_chunk);
        verify_addr += verify_chunk;
        verify_remaining -= verify_chunk;
    }
    
    // Compare CRC
    if (verify_crc == s_BootLoaderMgr.ReciveFileCRC32) {
        LOG_I("MCU flash verification passed (CRC32: 0x%08X)\r\n", verify_crc);
        return true;
    } else {
        LOG_E("MCU flash verification failed: expected CRC32 0x%08X, got 0x%08X\r\n",
              s_BootLoaderMgr.ReciveFileCRC32, verify_crc);
        return false;
    }
}

void BootLoader_SaveAppInfo()
{
    BootLoader_AppInfo_Typedef appInfo;
    Wireless_OTA_RxData_Typedef *otaRxData = App_Wireless_Get_OTA_RxData();
    appInfo.Ver[0] = otaRxData->TargetVer[0];
    appInfo.Ver[1] = otaRxData->TargetVer[1];
    appInfo.Ver[2] = otaRxData->TargetVer[2];
    appInfo.Ver[3] = otaRxData->TargetVer[3];
    appInfo.File_Size = s_BootLoaderMgr.ReceivedFileSize;
    appInfo.File_CRC32 = s_BootLoaderMgr.ReciveFileCRC32;
    appInfo.CRC32 = Bootloader_CRC32Update(0xFFFFFFFF, (uint8_t*)&appInfo, sizeof(appInfo) - sizeof(appInfo.CRC32));
    Drv_ExtFlash_Write(EXT_FLAG_APP1_INFO_ADDR, (uint8_t*)&appInfo, sizeof(appInfo));
}

/**
  * @brief  Main Bootloader Process (call in main loop)
  */
void App_Bootloader_Manager(void)
{
    static bool flash_initialized;
    static uint32_t mcu_write_offset;
    bool isConnected;
    Wireless_OTA_RxData_Typedef *otaRxData = App_Wireless_Get_OTA_RxData();
    uint8_t ReadWriteBuffer[128];
    switch (s_BootLoaderMgr.state)
    {
        case BOOT_STATE_INIT:
            LOG_I("Bootloader State: INIT\r");
            flash_initialized = false;
            mcu_write_offset = 0xFFFFFFFF;
            // Initialize any variables, prepare for connection
            Bootloader_ChangeState(BOOT_STATE_WAIT_CONNECT);
            break;
        case BOOT_STATE_WAIT_CONNECT:
            isConnected = App_Wireless_IsConnected();
            if (isConnected) {
                LOG_I("Bootloader State: WAIT_CONNECT -> WAIT_UPDATE\r\n");
                s_BootLoaderMgr.Connected = true;
                Bootloader_ChangeState(BOOT_STATE_WAIT_UPDATE);
            }
            break;
        case BOOT_STATE_WAIT_UPDATE:
            // Waiting for update command from host
            if(otaRxData->OTA_Req_BleMaxLen) {
                otaRxData->OTA_Req_BleMaxLen = false;
                s_BootLoader_TxData.ReplyBleMaxLen = true;
                s_BootLoader_TxData.PacketMaxLen = 128; // Ble MAx Length
            }

            if(otaRxData->OTA_Req_FileInfo) {
                otaRxData->OTA_Req_FileInfo = false;
                s_BootLoader_TxData.isAllowOTA = true;
                s_BootLoaderMgr.ReceivedFileSize = otaRxData->FileSize;
                s_BootLoaderMgr.ReciveFileCRC32 = otaRxData->FileCRC32;
                if(otaRxData->FileSize > TOTAL_APP_AREA/2) {
                    LOG_E("Received file size exceeds maximum limit\r\n");
                    s_BootLoader_TxData.isAllowOTA = false;
                }
                s_BootLoader_TxData.OTA_Req_FileInfo = true;
            }

            if(otaRxData->OTA_Req_OffsetInfo) {
                otaRxData->OTA_Req_OffsetInfo = false;
                // Process received OTA data packet  
                s_BootLoader_TxData.OTA_Req_OffsetInfo = true;
                s_BootLoader_TxData.FileOffset = (APP_START_ADDR & 0x000FFFFF);
                if(s_BootLoader_TxData.FileOffset == otaRxData->OTA_Offset) {
                    s_BootLoaderMgr.SavedFileSize = 0;
                    s_BootLoaderMgr.FlashStartAddr = EXT_APP1_START_ADDR;
                    s_BootLoaderMgr.CurrentAppArea = 1;  // Set CurrentAppArea based on FlashStartAddr
                    s_BootLoaderMgr.SavedFileCRC32 = 0;
                    Bootloader_ChangeState(BOOT_STATE_TRANSFER);
                    LOG_I("Bootloader State: WAIT_UPDATE -> TRANSFER\r\n");
                }
            }
            break;
        case BOOT_STATE_TRANSFER:
            if(otaRxData->OTA_Req_DataPacket) {
                otaRxData->OTA_Req_DataPacket = false;
                uint32_t expectedOffset = s_BootLoaderMgr.FlashStartAddr + s_BootLoaderMgr.SavedFileSize;
                if(expectedOffset%4096 == 0) {
                    LOG_D("Erasing flash sector at address: 0x%08X\r\n", expectedOffset);
                    Drv_ExtFlash_ErasePage(expectedOffset);
                    Drv_Delay(100);
                }
                // Here, write the received data to external flash
                Drv_ExtFlash_Write(expectedOffset,otaRxData->OTA_Data, otaRxData->OTA_DataLen);
                Drv_Delay(5); // Small delay to ensure write completion
                Drv_ExtFlash_Read(expectedOffset, ReadWriteBuffer, otaRxData->OTA_DataLen);
                if(memcmp(ReadWriteBuffer, otaRxData->OTA_Data, otaRxData->OTA_DataLen) != 0) {
                    LOG_E("Flash write verification failed at offset: 0x%08X\r\n", expectedOffset);
                    s_BootLoader_TxData.PackStatus = BL_PACK_S_ERR;
                    s_BootLoader_TxData.OTA_Req_DataPacket = true;
                    break;
                } else {
                    s_BootLoaderMgr.SavedFileCRC32 = Bootloader_CRC32Update(s_BootLoaderMgr.SavedFileCRC32, ReadWriteBuffer, otaRxData->OTA_DataLen);
                    s_BootLoader_TxData.PackStatus = BL_PACK_S_SUCCESS;
                    s_BootLoader_TxData.OTA_Req_DataPacket = true;
                }
                s_BootLoaderMgr.SavedFileSize += otaRxData->OTA_DataLen;
                // After writing all data, move to check firmware state
                if(s_BootLoaderMgr.SavedFileSize >= s_BootLoaderMgr.ReceivedFileSize) {
                    LOG_I("All data received, moving to CHECK_FIRMWARE\r\n");
                    Bootloader_ChangeState(BOOT_STATE_CHECK_FIRMWARE);
                }
            }
            break;
        case BOOT_STATE_CHECK_FIRMWARE:
            if(s_BootLoaderMgr.SavedFileSize < s_BootLoaderMgr.ReceivedFileSize) {
                LOG_E("Firmware size mismatch: received %lu, expected %lu\r\n",
                      s_BootLoaderMgr.SavedFileSize, s_BootLoaderMgr.ReceivedFileSize);
                Bootloader_ChangeState(BOOT_STATE_ERROR);
                s_BootLoader_TxData.OTA_Req_UpdateResult = true;
                s_BootLoader_TxData.UpdateResult = BL_UPDATE_S_DATALEN_ERR; // Size mismatch
                break;
            }
            if(s_BootLoaderMgr.SavedFileCRC32 == s_BootLoaderMgr.ReciveFileCRC32) {
                LOG_I("Firmware CRC32 check passed\r\n");
                BootLoader_SaveAppInfo();
                BootLoader_WriteBootFlag(s_BootLoaderMgr.CurrentAppArea, FLASH_FLAG_APP);
                Bootloader_ChangeState(BOOT_STATE_FLASH);
                flash_initialized = false;
                mcu_write_offset = 0xFFFFFFFF;  // Use 0xFFFFFFFF to indicate not started
            } else {
                LOG_E("Firmware CRC32 check failed\r\n");
                Bootloader_ChangeState(BOOT_STATE_ERROR);
                s_BootLoader_TxData.OTA_Req_UpdateResult = true;
                s_BootLoader_TxData.UpdateResult = BL_UPDATE_S_CRC_ERR; // CRC error
            }
            break;
        case BOOT_STATE_FLASH:
            // Initialize on first entry
            if (!flash_initialized) {
                LOG_I("Starting flash operation: Copying from ext flash 0x%08X to MCU flash 0x%08X, size: %lu bytes\r\n",
                      s_BootLoaderMgr.FlashStartAddr, APP_START_ADDR, s_BootLoaderMgr.SavedFileSize);
                
                // Determine CurrentAppArea
                if (!Bootloader_Flash_DetermineAppArea()) {
                    Bootloader_Flash_HandleError(BL_UPDATE_S_FLASH_ERR, &flash_initialized, &mcu_write_offset);
                    break;
                }
                
                // Validate file size
                if (!Bootloader_Flash_ValidateFileSize()) {
                    Bootloader_Flash_HandleError(BL_UPDATE_S_DATALEN_ERR, &flash_initialized, &mcu_write_offset);
                    break;
                }
                
                // Erase required pages
                if (!Bootloader_Flash_ErasePages()) {
                    Bootloader_Flash_HandleError(BL_UPDATE_S_FLASH_ERR, &flash_initialized, &mcu_write_offset);
                    break;
                }
                
                flash_initialized = true;
                mcu_write_offset = 0;
            }
            
            // Copy data from external flash to MCU flash in chunks
            if (mcu_write_offset < s_BootLoaderMgr.SavedFileSize) {
                if (!Bootloader_Flash_CopyChunk(&mcu_write_offset)) {
                    Bootloader_Flash_HandleError(BL_UPDATE_S_FLASH_ERR, &flash_initialized, &mcu_write_offset);
                    break;
                }
            }
            
            // Check if all data has been copied and verify
            if (mcu_write_offset >= s_BootLoaderMgr.SavedFileSize) {
                if (Bootloader_Flash_VerifyData()) {
                    LOG_I("Firmware flash completed successfully\r\n");
                    s_BootLoader_TxData.OTA_Req_UpdateResult = true;
                    s_BootLoader_TxData.UpdateResult = BL_UPDATE_S_SUCCESS;
                    flash_initialized = false;
                    mcu_write_offset = 0xFFFFFFFF;
                    Bootloader_ChangeState(BOOT_STATE_JUMP_APP);
                } else {
                    Bootloader_Flash_HandleError(BL_UPDATE_S_CRC_ERR, &flash_initialized, &mcu_write_offset);
                }
            }
            break;
        case BOOT_STATE_ERROR:
            break;
        default:
            s_BootLoaderMgr.state = BOOT_STATE_INIT;
            break;
    }
}

/**
  * @brief  Jump to Application
  */
void App_Bootloader_JumpToApp(uint32_t appAddress)
{
    uint32_t JumpAddress;
    pFunction JumpToApplication;

    /* Check if the Stack Pointer is valid */
    if (((*(__IO uint32_t*)appAddress) & 0x2FFE0000 ) == 0x20000000)
    {
        /* Jump to user application */
        JumpAddress = *(__IO uint32_t*) (appAddress + 4);
        JumpToApplication = (pFunction) JumpAddress;

        /* Initialize user application's Stack Pointer */
        __set_MSP(*(__IO uint32_t*) appAddress);
        
        /* Disable interrupts */
        __disable_irq();

        /* Jump to application */
        JumpToApplication();
    }
}

BootLoader_TxData_Typedef *App_Bootloader_Get_TxData(void)
{
    return &s_BootLoader_TxData;
}


