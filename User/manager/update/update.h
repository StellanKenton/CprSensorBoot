/***********************************************************************************
* @file     : update.h
* @brief    : Update manager declarations.
* @details  : Provides a minimal bootloader update state holder.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_UPDATE_H
#define CPRSENSORBOOT_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_FLASH_MEM_BOOT_FLAG             760U
#define APP_FLASH_APP1_START_SECTOR         768U
#define APP_FLASH_APP2_START_SECTOR         896U
#define APP_FLASH_BOOT_FLAG_SECTOR          760U
#define APP_FLASH_SECTOR_SIZE               4096U
#define APP_FLASH_PAGE_SIZE                 256U
#define APP_FLASH_APP_CRC_RESERVED_SIZE     4096U
#define APP_FLASH_PACKET_DATA_MAX           64U
#define APP_FLASH_REPLY_DATA_MAX            16U

#define APP_FLASH_APP1_BASE_ADDR            (APP_FLASH_APP1_START_SECTOR << 12)
#define APP_FLASH_APP1_DATA_ADDR            (APP_FLASH_APP1_BASE_ADDR + APP_FLASH_APP_CRC_RESERVED_SIZE)
#define APP_FLASH_APP2_BASE_ADDR            (APP_FLASH_APP2_START_SECTOR << 12)
#define APP_FLASH_BOOT_FLAG_ADDR            (APP_FLASH_BOOT_FLAG_SECTOR << 12)
#define APP_FLASH_APP1_REGION_SIZE          ((APP_FLASH_APP2_START_SECTOR - APP_FLASH_APP1_START_SECTOR) * APP_FLASH_SECTOR_SIZE)
#define APP_FLASH_APP1_DATA_MAX_SIZE        (APP_FLASH_APP1_REGION_SIZE - APP_FLASH_APP_CRC_RESERVED_SIZE)
#define APP_FLASH_BLE_FRAME_MAX             64U
#define APP_FLASH_FRAME_OVERHEAD            8U
#define APP_FLASH_AES_ALIGN                 16U
#define APP_FLASH_OTA_PACKET_MAX            48U
#define APP_FLASH_OTA_CHUNK_OVERHEAD        6U
#define APP_FLASH_OTA_CHUNK_MAX             (APP_FLASH_OTA_PACKET_MAX - APP_FLASH_OTA_CHUNK_OVERHEAD)


typedef enum eUpdateState {
    E_UPDATE_STATE_UNINIT = 0,
    E_UPDATE_STATE_IDLE,
    E_UPDATE_STATE_CHECK,
    E_UPDATE_STATE_UPDATE,
    E_UPDATE_STATE_RESTORE,
    E_UPDATE_STATE_MAX,
} eUpdateState;

typedef struct stUpdateStatus {
    eUpdateState state;
    bool isUpdateRequested;
    uint32_t lastProcessTick;
} stUpdateStatus;

void updateReset(void);
bool updateInit(void);
void updateProcess(uint32_t nowTick);
const stUpdateStatus *updateGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_UPDATE_H
/**************************End of file********************************/
