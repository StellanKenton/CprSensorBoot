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
#define APP_FLASH_TOTAL_SIZE                (4UL * 1024UL * 1024UL)
#define APP_FLASH_APP2_REGION_SIZE          (APP_FLASH_TOTAL_SIZE - APP_FLASH_APP2_BASE_ADDR)
#define APP_FLASH_APP2_DATA_ADDR            (APP_FLASH_APP2_BASE_ADDR + APP_FLASH_APP_CRC_RESERVED_SIZE)
#define APP_FLASH_APP2_DATA_MAX_SIZE        (APP_FLASH_APP2_REGION_SIZE - APP_FLASH_APP_CRC_RESERVED_SIZE)
#define APP_FLASH_BLE_FRAME_MAX             64U
#define APP_FLASH_FRAME_OVERHEAD            8U
#define APP_FLASH_AES_ALIGN                 16U
#define APP_FLASH_OTA_PACKET_MAX            48U
#define APP_FLASH_OTA_CHUNK_OVERHEAD        6U
#define APP_FLASH_OTA_CHUNK_MAX             (APP_FLASH_OTA_PACKET_MAX - APP_FLASH_OTA_CHUNK_OVERHEAD)

#define UPDATE_IMAGE_MAGIC                  0x55504454UL
#define UPDATE_BOOT_RECORD_MAGIC            0x4254464CUL
#define UPDATE_HEADER_VERSION               0x00000001UL

#define UPDATE_MCU_APP_START_ADDR           0x08020000UL
#define UPDATE_MCU_APP_SIZE                 0x00060000UL

#define UPDATE_CRC32_INIT_VALUE             0xFFFFFFFFUL


typedef enum eUpdateState {
    E_UPDATE_STATE_UNINIT = 0,
    E_UPDATE_STATE_IDLE,
    E_UPDATE_STATE_CHECK_REQUEST,
    E_UPDATE_STATE_VALIDATE_APP2,
    E_UPDATE_STATE_PREPARE_APP1,
    E_UPDATE_STATE_BACKUP_APP_TO_APP1,
    E_UPDATE_STATE_VERIFY_APP1,
    E_UPDATE_STATE_ERASE_MCU_APP,
    E_UPDATE_STATE_PROGRAM_APP2_TO_MCU,
    E_UPDATE_STATE_VERIFY_MCU_APP,
    E_UPDATE_STATE_CLEAR_FLAG,
    E_UPDATE_STATE_ROLLBACK_ERASE_MCU_APP,
    E_UPDATE_STATE_ROLLBACK_APP1_TO_MCU,
    E_UPDATE_STATE_VERIFY_ROLLBACK,
    E_UPDATE_STATE_JUMP_TO_APP,
    E_UPDATE_STATE_ERROR,
    E_UPDATE_STATE_MAX,
} eUpdateState;

typedef enum eUpdateBootFlag {
    E_UPDATE_BOOT_FLAG_IDLE = 0,
    E_UPDATE_BOOT_FLAG_APP_REQUEST,
    E_UPDATE_BOOT_FLAG_BACKUP_DONE,
    E_UPDATE_BOOT_FLAG_PROGRAM_DONE,
    E_UPDATE_BOOT_FLAG_SUCCESS,
    E_UPDATE_BOOT_FLAG_FAILED,
} eUpdateBootFlag;

typedef enum eUpdateImageState {
    E_UPDATE_IMAGE_STATE_EMPTY = 0,
    E_UPDATE_IMAGE_STATE_RECEIVING,
    E_UPDATE_IMAGE_STATE_READY,
    E_UPDATE_IMAGE_STATE_INVALID,
} eUpdateImageState;

typedef enum eUpdateError {
    E_UPDATE_ERROR_NONE = 0,
    E_UPDATE_ERROR_APP2_HEADER_INVALID = 1,
    E_UPDATE_ERROR_APP2_CRC_MISMATCH = 2,
    E_UPDATE_ERROR_APP1_BACKUP_WRITE_FAILED = 3,
    E_UPDATE_ERROR_APP1_CRC_MISMATCH = 4,
    E_UPDATE_ERROR_MCU_APP_ERASE_FAILED = 5,
    E_UPDATE_ERROR_MCU_APP_PROGRAM_FAILED = 6,
    E_UPDATE_ERROR_MCU_APP_CRC_MISMATCH = 7,
    E_UPDATE_ERROR_ROLLBACK_FAILED = 8,
    E_UPDATE_ERROR_BOOT_RECORD_INVALID = 9,
    E_UPDATE_ERROR_FLASH_ACCESS_FAILED = 10,
    E_UPDATE_ERROR_APP_VECTOR_INVALID = 11,
    E_UPDATE_ERROR_MCU_FLASH_ACCESS_FAILED = 12,
} eUpdateError;

#define UPDATE_TEST_FORCE_APP_REQUEST_ENABLE    0U

#pragma pack(push, 1)
typedef struct stUpdateImageHeader {
    uint32_t magic;
    uint32_t headerVersion;
    uint32_t imageVersion;
    uint32_t imageSize;
    uint32_t imageCrc32;
    uint32_t writeOffset;
    uint32_t imageState;
    uint32_t reserved[9];
} stUpdateImageHeader;

typedef struct stUpdateBootRecord {
    uint32_t magic;
    uint32_t requestFlag;
    uint32_t lastError;
    uint32_t app1Crc32;
    uint32_t app2Crc32;
    uint32_t appSize;
    uint32_t sequence;
    uint32_t reserved[9];
} stUpdateBootRecord;
#pragma pack(pop)

typedef struct stUpdateStatus {
    eUpdateState state;
    bool isUpdateRequested;
    bool isRollbackActive;
    eUpdateBootFlag requestFlag;
    eUpdateError lastError;
    uint32_t lastProcessTick;
    uint32_t currentOffset;
    uint32_t totalSize;
    uint32_t activeCrc32;
} stUpdateStatus;

void updateReset(void);
bool updateInit(void);
void updateProcess(uint32_t nowTick);
const stUpdateStatus *updateGetStatus(void);
bool updateGetBootRecord(stUpdateBootRecord *record);
bool updateHasNormalAppBootFlag(void);
bool updateJumpToAppIfValid(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_UPDATE_H
/**************************End of file********************************/
