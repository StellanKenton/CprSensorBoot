#ifndef __APP_BOOTLOADER_H
#define __APP_BOOTLOADER_H

#include "main.h"
#include "stdbool.h"

// Define Application Start Address
// Adjust this according to your linker script and vector table offset

typedef enum {
    FLASH_FLAG_APP1 = 1,
    FLASH_FLAG_APP2 = 2,
} Bootloader_BootFlagArea_EnumDef;

typedef enum {
    FLASH_FLAG_BLK = 0xA0,
    FLASH_FLAG_APP = 0xA1,
    FLASH_FLAG_BK  = 0xA2,
    FLASH_FLAG_UP  = 0xA3,
} Bootloader_BootFlag_EnumDef;

typedef struct {
    uint8_t Reserve1;
    uint8_t AppArea1;
    uint8_t Reserve2;
    uint8_t AppArea2;
    uint32_t CRC_Data;
} Bootloader_BootFlag_t;



/******************************
 * 分区总览 (相对于Flash基址0x080的偏移) MCU端
 * 1. Bootloader:   96KB  (0x00000 - 0x17FFF) 大小: 0x18000
 * 2. Boot_Reserve: 4KB   (0x18000 - 0x19FFF) 大小: 0x1000
 * 3. App:          384KB (0x1A000 - 0x7FFFF) 大小: 0x60000
 ******************************/

/***********************************************
 * 分区详细划分 (相对于Flash基址0x000 的偏移) 外置FLASH端
 * 1. Flag Area:         32KB  (0x2F8000 - 0x2FFFFF ) 大小: 0x0800
 * 2. App Area1:         512KB (0x300000 - 0x37FFFF ) 大小: 0x80000
 * 3. App Area2:         512KB (0x380000 - 0x3FFFFF ) 大小: 0x80000
 *******************************************/

/* MCU内置Flash 分区：Bootloader 0x00000-0x17FFF，Reserve 0x18000-0x19FFF，App 0x1A000-0x7FFFF */
#define BOOTLOADER_START_ADDR    0x08000000
#define BOOTLOADER_END_ADDR      0x08017FFF
#define BOOTLOADER_SIZE          (BOOTLOADER_END_ADDR - BOOTLOADER_START_ADDR + 1)

#define BOOT_RESERVE_START_ADDR  0x08018000
#define BOOT_RESERVE_END_ADDR    0x08019FFF
#define BOOT_RESERVE_SIZE        (BOOT_RESERVE_END_ADDR - BOOT_RESERVE_START_ADDR + 1)

#define APP_START_ADDR           0x0801A000
#define APP_END_ADDR             0x0807FFFF
#define APP_SIZE                 (APP_END_ADDR - APP_START_ADDR + 1)

/* 外置Flash 分区：Flag 0x2F8000-0x2FFFFF，App1 0x300000-0x37FFFF，App2 0x380000-0x3FFFFF */
#define EXT_FLAG_START_ADDR      0x002F8000
#define EXT_FLAG_BACKUP_ADDR     0x002F9000
#define EXT_FLAG_APP1_INFO_ADDR  0x002FA000
#define EXT_FLAG_APP2_INFO_ADDR  0x002FB000
#define EXT_FLAG_END_ADDR        0x002FFFFF
#define EXT_FLAG_SIZE            (EXT_FLAG_END_ADDR - EXT_FLAG_START_ADDR + 1)

#define EXT_APP1_START_ADDR      0x00300000
#define EXT_APP1_END_ADDR        0x0037FFFF
#define EXT_APP1_SIZE            (EXT_APP1_END_ADDR - EXT_APP1_START_ADDR + 1)

#define EXT_APP2_START_ADDR      0x00380000
#define EXT_APP2_END_ADDR        0x003FFFFF
#define EXT_APP2_SIZE            (EXT_APP2_END_ADDR - EXT_APP2_START_ADDR + 1)

/* 计算各区域大小 */
#define TOTAL_BOOTLOADER_AREA    (BOOTLOADER_SIZE + BOOT_RESERVE_SIZE)
#define TOTAL_APP_AREA           (APP_SIZE)
#define TOTAL_USED_AREA          (TOTAL_BOOTLOADER_AREA + TOTAL_APP_AREA)



/*********************************************************************************************************************/
typedef enum {
    BOOT_STATE_INIT = 0,
    BOOT_STATE_WAIT_CONNECT,  // Waiting for connection
    BOOT_STATE_WAIT_UPDATE,   // Waiting for update command
    BOOT_STATE_TRANSFER,      // Transferring firmware to external flash
    BOOT_STATE_CHECK_FIRMWARE,// Checking firmware integrity
    BOOT_STATE_FLASH,         // Flashing new firmware
    BOOT_STATE_JUMP_APP,      // Jumping to application
    BOOT_STATE_ERROR            // Error state
} BootloaderState_t;

typedef struct {
    BootloaderState_t state;
    uint32_t timestamp;
    Bootloader_BootFlag_t boot_flag;
    bool NeedUpdate;
    bool Connected;

    uint8_t  CurrentAppArea;
    uint8_t  CurrentBackupArea;
    uint32_t FlashStartAddr;
    uint32_t ReceivedFileSize;
    uint32_t SavedFileSize;
    uint32_t ReciveFileCRC32;
    uint32_t SavedFileCRC32;
} BootloaderMgr_t;

typedef struct {
    uint8_t Ver[4];
    uint32_t File_CRC32;
    uint32_t File_Size;
    uint32_t CRC32;
} BootLoader_AppInfo_Typedef;


typedef enum {
    BL_FILE_S_NORMAL = 0,
    BL_FILE_S_LOW_VERSION,
    BL_FILE_S_OUT_OF_SIZE,
    BL_FILE_S_MAX,
} BootLoader_FileState_EnumDef;

typedef enum {
    BL_UPDATE_S_SUCCESS = 0,
    BL_UPDATE_S_DATALEN_ERR,
    BL_UPDATE_S_CRC_ERR,
    BL_UPDATE_S_FLASH_ERR,
    BL_UPDATE_S_MAX,
}BootLoader_UpdateResult_e;

typedef enum {
    BL_PACK_S_SUCCESS = 0,
    BL_PACK_S_PACK_ERR,
    BL_PACK_S_LEN_ERR,
    BL_PACK_S_CRC_ERR,
    BL_PACK_S_ERR,
    BL_PACK_S_MAX
} BootLoader_PackState_EnumDef;

typedef struct {
    bool RequestFlag;
    bool OTA_Req_FileInfo;
    bool PacketFlag;
    bool ReplyBleMaxLen;
    bool OTA_Req_OffsetInfo;
    bool OTA_Req_DataPacket;
    bool OTA_Req_UpdateResult;

    bool isAllowOTA;
    uint8_t Ver[4];
    uint8_t PackStatus;
    uint8_t UpdateResult;
    uint16_t PacketMaxLen;
    uint32_t SavedDataLen;
    uint32_t FileOffset;
} BootLoader_TxData_Typedef;

typedef void (*pFunction)(void);

void App_Bootloader_Init(void);
void App_Bootloader_Manager(void);
void App_Bootloader_JumpToApp(uint32_t appAddress);
void App_BootLoader_JumpCheck(void);
bool App_Bootloader_GetNeedUpdate(void);
BootLoader_TxData_Typedef *App_Bootloader_Get_TxData(void);
#endif // __APP_BOOTLOADER_H
