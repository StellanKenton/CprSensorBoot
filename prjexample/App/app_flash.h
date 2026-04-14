/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_flash.h
* \brief OTA flash update state machine for APP1 image storage.
* \date 2026-04-13
**/
#ifndef APP_FLASH_H
#define APP_FLASH_H

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"
#include "log.h"

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

extern osMessageQueueId_t OTA_FlashHandle;

typedef uint64_t Bootloader_Flag_EnumDef;

#define BOOTLADER_FLAG_APP                  UINT64_C(0xA512345678A1)
#define BOOTLADER_FLAG_BOOT                 UINT64_C(0xA512345678A2)
#define BOOTLADER_FLAG_UPDATE               UINT64_C(0xA512345678A3)
#define BOOTLADER_FLAG_RESTORE              UINT64_C(0xA512345678A4)

typedef enum {
    APP_FLASH_STATE_IDLE = 0,
    APP_FLASH_STATE_READY,
    APP_FLASH_STATE_RECEIVING,
    APP_FLASH_STATE_VERIFYING,
    APP_FLASH_STATE_WAIT_REBOOT,
    APP_FLASH_STATE_ERROR,
} App_Flash_State_EnumDef;

typedef enum {
    APP_FLASH_RESULT_OK = 0x00,
    APP_FLASH_RESULT_BUSY = 0x01,
    APP_FLASH_RESULT_INVALID = 0x02,
    APP_FLASH_RESULT_NOT_READY = 0x03,
    APP_FLASH_RESULT_FLASH_ERR = 0x04,
    APP_FLASH_RESULT_CRC_ERR = 0x05,
    APP_FLASH_RESULT_OFFSET_ERR = 0x06,
    APP_FLASH_RESULT_SIZE_ERR = 0x07,
    APP_FLASH_RESULT_QUEUE_ERR = 0x08,
} App_Flash_Result_EnumDef;

typedef struct {
    uint8_t cmd;
    uint16_t len;
    uint8_t data[APP_FLASH_PACKET_DATA_MAX];
} App_Flash_Packet_TypeDef;

typedef struct {
    uint8_t cmd;
    uint16_t len;
    uint8_t data[APP_FLASH_REPLY_DATA_MAX];
} App_Flash_Reply_TypeDef;

#define APP_FLASH_IMAGE_HEADER_MAGIC        0x41503148UL
#define APP_FLASH_VERIFY_CHUNK_SIZE         128U
#define APP_FLASH_REBOOT_DELAY_MS           500U
#define APP_FLASH_HANDLE_PERIOD_MS          30U
#define APP_FLASH_BOOT_FLAG_SIZE            ((uint16_t)sizeof(Bootloader_Flag_EnumDef))

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t image_size;
    uint32_t image_crc32;
    uint8_t reserved[6];
    uint16_t checksum;
} App_Flash_ImageHeader_TypeDef;
#pragma pack(pop)

typedef struct {
    App_Flash_State_EnumDef state;
    App_Flash_Result_EnumDef last_result;
    uint8_t file_version[4];
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t expected_offset;
    uint32_t received_crc32;
    uint16_t expected_packet_no;
    uint16_t reboot_delay_ms;
    uint8_t reply_pending;
    uint8_t reboot_armed;
    App_Flash_Reply_TypeDef reply;
} App_Flash_Control_TypeDef;

void App_Flash_Init(void);
void App_Flash_Handle(void);
bool App_Flash_PushPacket(uint8_t cmd, const uint8_t *data, uint16_t len);
bool App_Flash_FetchReply(App_Flash_Reply_TypeDef *reply);
void App_Flash_NotifyReplySent(void);
uint32_t App_Flash_GetExpectedOffset(void);
uint16_t App_Flash_GetMaxPacketLength(void);
App_Flash_State_EnumDef App_Flash_GetState(void);

#ifdef __cplusplus
}
#endif
#endif  // APP_FLASH_H
