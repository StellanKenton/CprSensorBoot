/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_flash.c
* \brief OTA flash update state machine for APP1 image storage.
* \date 2026-04-13
**/
#include "app_flash.h"

#include "drv_flash.h"
#include "lib_comm.h"
#include "app_power.h"
#include "app_system.h"
#include "log.h"

static App_Flash_Control_TypeDef s_flash_ctrl;

static const uint8_t s_app_flash_version[4] = {
    SoftWare_Version,
    SoftSub_Version,
    SoftBuild_Version,
    0x00U,
};

static void App_Flash_ResetSession(void);
static void App_Flash_SetReply(uint8_t cmd, const uint8_t *data, uint16_t len);
static uint32_t App_Flash_ReadU32LE(const uint8_t *data);
static uint16_t App_Flash_ReadU16LE(const uint8_t *data);
static void App_Flash_WriteU16LE(uint8_t *data, uint16_t value);
static void App_Flash_WriteU32LE(uint8_t *data, uint32_t value);
static void App_Flash_WriteU64LE(uint8_t *data, uint64_t value);
static HAL_StatusTypeDef App_Flash_WriteBuffer(uint32_t addr, const uint8_t *data, uint16_t len);
static HAL_StatusTypeDef App_Flash_ReadBuffer(uint32_t addr, uint8_t *data, uint16_t len);
static HAL_StatusTypeDef App_Flash_EraseRange(uint32_t start_addr, uint32_t end_addr);
static uint32_t App_Flash_Crc32Update(uint32_t crc, const uint8_t *data, uint32_t len);
static uint8_t App_Flash_CompareVersion(const uint8_t *lhs, const uint8_t *rhs);
static uint16_t App_Flash_CalcChunkCrc16(const uint8_t *data, uint16_t len);
static bool App_Flash_HandleRequest(const App_Flash_Packet_TypeDef *packet);
static bool App_Flash_HandleFileInfo(const App_Flash_Packet_TypeDef *packet);
static bool App_Flash_HandleOffset(const App_Flash_Packet_TypeDef *packet);
static bool App_Flash_HandleData(const App_Flash_Packet_TypeDef *packet);
static bool App_Flash_HandleResult(const App_Flash_Packet_TypeDef *packet);
static bool App_Flash_VerifyImage(void);
static bool App_Flash_WriteImageHeader(void);
static bool App_Flash_WriteBootFlag(Bootloader_Flag_EnumDef boot_flag);

static void App_Flash_ResetSession(void)
{
    memset(&s_flash_ctrl, 0, sizeof(s_flash_ctrl));
    s_flash_ctrl.state = APP_FLASH_STATE_IDLE;
    s_flash_ctrl.last_result = APP_FLASH_RESULT_OK;
    s_flash_ctrl.received_crc32 = 0xFFFFFFFFUL;
}

static void App_Flash_SetReply(uint8_t cmd, const uint8_t *data, uint16_t len)
{
    s_flash_ctrl.reply.cmd = cmd;
    s_flash_ctrl.reply.len = 0U;
    if ((data != NULL) && (len > 0U)) {
        if (len > APP_FLASH_REPLY_DATA_MAX) {
            len = APP_FLASH_REPLY_DATA_MAX;
        }
        memcpy(s_flash_ctrl.reply.data, data, len);
        s_flash_ctrl.reply.len = len;
    }
    s_flash_ctrl.reply_pending = 1U;
}

static uint32_t App_Flash_ReadU32LE(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint16_t App_Flash_ReadU16LE(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void App_Flash_WriteU16LE(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)(value >> 8);
}

static void App_Flash_WriteU32LE(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8) & 0xFFU);
    data[2] = (uint8_t)((value >> 16) & 0xFFU);
    data[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void App_Flash_WriteU64LE(uint8_t *data, uint64_t value)
{
    uint8_t index;

    for (index = 0U; index < APP_FLASH_BOOT_FLAG_SIZE; index++) {
        data[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static uint8_t App_Flash_CompareVersion(const uint8_t *lhs, const uint8_t *rhs)
{
    uint8_t index;

    for (index = 0U; index < 4U; index++) {
        if (lhs[index] > rhs[index]) {
            return 1U;
        }
        if (lhs[index] < rhs[index]) {
            return 0U;
        }
    }

    return 2U;
}

static uint16_t App_Flash_CalcChunkCrc16(const uint8_t *data, uint16_t len)
{
    return Crc16Compute(data, len);
}

static uint8_t App_Flash_IsCrossPage(uint32_t start_addr, uint16_t len)
{
    uint32_t page_mask = APP_FLASH_PAGE_SIZE - 1U;
    return ((start_addr & ~page_mask) != ((start_addr + len - 1U) & ~page_mask));
}

static void App_Flash_GetPageSegments(uint32_t start_addr, uint16_t len, uint16_t *first_seg_len, uint16_t *second_seg_len)
{
    uint32_t remaining = APP_FLASH_PAGE_SIZE - (start_addr & (APP_FLASH_PAGE_SIZE - 1U));
    *first_seg_len = (len <= remaining) ? len : (uint16_t)remaining;
    *second_seg_len = len - *first_seg_len;
}

static HAL_StatusTypeDef App_Flash_WriteBuffer(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t first_seg_len;
    uint16_t second_seg_len;

    if ((data == NULL) || (len == 0U)) {
        return HAL_ERROR;
    }

    if (App_Flash_IsCrossPage(addr, len) == 0U) {
        return Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr, (uint8_t *)data, len);
    }

    App_Flash_GetPageSegments(addr, len, &first_seg_len, &second_seg_len);
    if (Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr, (uint8_t *)data, first_seg_len) != HAL_OK) {
        return HAL_ERROR;
    }
    osDelay(5);
    return Drv_GD25Q32_WritePage(&GD25Q32_Dev, addr + first_seg_len, (uint8_t *)(data + first_seg_len), second_seg_len);
}

static HAL_StatusTypeDef App_Flash_ReadBuffer(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint16_t read_len;

    if ((data == NULL) || (len == 0U)) {
        return HAL_ERROR;
    }

    while (len > 0U) {
        read_len = (len > APP_FLASH_PAGE_SIZE) ? APP_FLASH_PAGE_SIZE : len;
        if (Drv_GD25Q32_ReadPage(&GD25Q32_Dev, addr, data, read_len) != HAL_OK) {
            return HAL_ERROR;
        }
        addr += read_len;
        data += read_len;
        len -= read_len;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef App_Flash_EraseRange(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t addr = start_addr;

    while (addr < end_addr) {
        if (Drv_GD25Q32_EraseSector(&GD25Q32_Dev, addr) != HAL_OK) {
            return HAL_ERROR;
        }
        osDelay(200);
        addr += APP_FLASH_SECTOR_SIZE;
    }

    return HAL_OK;
}

static uint32_t App_Flash_Crc32Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t index;
    uint8_t bit;

    for (index = 0; index < len; index++) {
        crc ^= data[index];
        for (bit = 0; bit < 8U; bit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static bool App_Flash_HandleRequest(const App_Flash_Packet_TypeDef *packet)
{
    uint8_t reply_data[9] = {0};
    uint16_t requested_len;
    uint16_t packet_len;

    if (packet->len < 2U) {
        return false;
    }

    requested_len = App_Flash_ReadU16LE(packet->data);
    packet_len = App_Flash_GetMaxPacketLength();
    if ((requested_len != 0U) && (requested_len < packet_len)) {
        packet_len = requested_len;
    }

    reply_data[0] = 0x00U;
    memcpy(&reply_data[1], s_app_flash_version, sizeof(s_app_flash_version));
    App_Flash_WriteU16LE(&reply_data[5], packet_len);
    App_Flash_WriteU16LE(&reply_data[7], packet_len);

    if (s_flash_ctrl.state == APP_FLASH_STATE_IDLE) {
        s_flash_ctrl.state = APP_FLASH_STATE_READY;
    }
    eSystemMode = E_SYSTEM_UPDATE_MODE;
    App_Flash_SetReply(E_CMD_OTA_REQUEST, reply_data, sizeof(reply_data));
    return true;
}

static bool App_Flash_HandleFileInfo(const App_Flash_Packet_TypeDef *packet)
{
    uint32_t erase_end_addr;
    uint8_t reply_data[9] = {0};
    uint8_t version_cmp;
    uint8_t incoming_version[4];
    uint32_t incoming_size;
    uint32_t incoming_crc32;

    if (packet->len < 12U) {
        reply_data[0] = APP_FLASH_RESULT_INVALID;
        App_Flash_SetReply(E_CMD_OTA_FILE_INFO, reply_data, 1U);
        s_flash_ctrl.state = APP_FLASH_STATE_ERROR;
        return false;
    }

    memcpy(incoming_version, packet->data, sizeof(incoming_version));
    incoming_size = App_Flash_ReadU32LE(&packet->data[4]);
    incoming_crc32 = App_Flash_ReadU32LE(&packet->data[8]);

    version_cmp = App_Flash_CompareVersion(incoming_version, s_app_flash_version);
    if ((version_cmp == 0U) || (version_cmp == 2U)) {
        reply_data[0] = 0x01U;
        App_Flash_SetReply(E_CMD_OTA_FILE_INFO, reply_data, sizeof(reply_data));
        return false;
    }

    if ((incoming_size == 0U) || (incoming_size > APP_FLASH_APP1_DATA_MAX_SIZE)) {
        reply_data[0] = 0x02U;
        App_Flash_SetReply(E_CMD_OTA_FILE_INFO, reply_data, sizeof(reply_data));
        return false;
    }

    if ((s_flash_ctrl.state == APP_FLASH_STATE_RECEIVING) &&
        (memcmp(incoming_version, s_flash_ctrl.file_version, sizeof(incoming_version)) == 0) &&
        (s_flash_ctrl.image_size == incoming_size) &&
        (s_flash_ctrl.image_crc32 == incoming_crc32)) {
        reply_data[0] = 0x00U;
        App_Flash_WriteU32LE(&reply_data[1], s_flash_ctrl.expected_offset);
        App_Flash_WriteU32LE(&reply_data[5], s_flash_ctrl.received_crc32 ^ 0xFFFFFFFFUL);
        App_Flash_SetReply(E_CMD_OTA_FILE_INFO, reply_data, sizeof(reply_data));
        return true;
    }

    App_Flash_ResetSession();
    memcpy(s_flash_ctrl.file_version, incoming_version, sizeof(s_flash_ctrl.file_version));
    s_flash_ctrl.image_size = incoming_size;
    s_flash_ctrl.image_crc32 = incoming_crc32;
    s_flash_ctrl.state = APP_FLASH_STATE_READY;
    s_flash_ctrl.reboot_armed = 0U;

    erase_end_addr = APP_FLASH_APP1_DATA_ADDR + s_flash_ctrl.image_size;
    erase_end_addr = (erase_end_addr + APP_FLASH_SECTOR_SIZE - 1U) & ~(APP_FLASH_SECTOR_SIZE - 1U);

    if (App_Flash_EraseRange(APP_FLASH_APP1_BASE_ADDR, erase_end_addr) != HAL_OK) {
        reply_data[0] = APP_FLASH_RESULT_FLASH_ERR;
        App_Flash_SetReply(E_CMD_OTA_FILE_INFO, reply_data, 1U);
        s_flash_ctrl.state = APP_FLASH_STATE_ERROR;
        return false;
    }

    s_flash_ctrl.state = APP_FLASH_STATE_RECEIVING;
    s_flash_ctrl.received_crc32 = 0xFFFFFFFFUL;
    reply_data[0] = 0x00U;
    App_Flash_SetReply(E_CMD_OTA_FILE_INFO, reply_data, sizeof(reply_data));
    return true;
}

static bool App_Flash_HandleOffset(const App_Flash_Packet_TypeDef *packet)
{
    uint8_t offset_data[4];
    (void)packet;

    App_Flash_WriteU32LE(offset_data, s_flash_ctrl.expected_offset);

    if ((s_flash_ctrl.state != APP_FLASH_STATE_RECEIVING) && (s_flash_ctrl.state != APP_FLASH_STATE_READY)) {
        App_Flash_SetReply(E_CMD_OTA_OFFSET, offset_data, sizeof(offset_data));
        return false;
    }

    App_Flash_SetReply(E_CMD_OTA_OFFSET, offset_data, sizeof(offset_data));
    return true;
}

static bool App_Flash_HandleData(const App_Flash_Packet_TypeDef *packet)
{
    uint16_t packet_no;
    uint16_t chunk_len;
    uint16_t chunk_crc;
    uint32_t write_addr;
    uint8_t reply_state;

    if (s_flash_ctrl.state != APP_FLASH_STATE_RECEIVING) {
        reply_state = 0x04U;
        App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
        return false;
    }

    if (packet->len < 6U) {
        reply_state = 0x02U;
        App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
        return false;
    }

    packet_no = App_Flash_ReadU16LE(packet->data);
    chunk_len = App_Flash_ReadU16LE(&packet->data[2]);
    chunk_crc = App_Flash_ReadU16LE(&packet->data[4]);

    if ((chunk_len == 0U) || ((uint16_t)(chunk_len + APP_FLASH_OTA_CHUNK_OVERHEAD) != packet->len)) {
        reply_state = 0x02U;
        App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
        return false;
    }

    if ((packet_no != s_flash_ctrl.expected_packet_no) || (chunk_len > APP_FLASH_OTA_CHUNK_MAX)) {
        reply_state = 0x01U;
        App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
        return false;
    }

    if ((s_flash_ctrl.expected_offset + chunk_len) > s_flash_ctrl.image_size) {
        reply_state = 0x02U;
        App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
        return false;
    }

    if (App_Flash_CalcChunkCrc16(&packet->data[6], chunk_len) != chunk_crc) {
        reply_state = 0x03U;
        App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
        return false;
    }

    write_addr = APP_FLASH_APP1_DATA_ADDR + s_flash_ctrl.expected_offset;
    if (App_Flash_WriteBuffer(write_addr, &packet->data[6], chunk_len) != HAL_OK) {
        s_flash_ctrl.state = APP_FLASH_STATE_ERROR;
        reply_state = 0x04U;
        App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
        return false;
    }

    s_flash_ctrl.expected_offset += chunk_len;
    s_flash_ctrl.expected_packet_no++;
    s_flash_ctrl.received_crc32 = App_Flash_Crc32Update(s_flash_ctrl.received_crc32, &packet->data[6], chunk_len);
    reply_state = 0x00U;
    App_Flash_SetReply(E_CMD_OTA_DATA, &reply_state, 1U);
    return true;
}

static bool App_Flash_VerifyImage(void)
{
    uint8_t verify_buffer[APP_FLASH_VERIFY_CHUNK_SIZE];
    uint32_t remain_len = s_flash_ctrl.image_size;
    uint32_t read_addr = APP_FLASH_APP1_DATA_ADDR;
    uint16_t read_len;
    uint32_t crc32 = 0xFFFFFFFFUL;

    while (remain_len > 0U) {
        read_len = (remain_len > APP_FLASH_VERIFY_CHUNK_SIZE) ? APP_FLASH_VERIFY_CHUNK_SIZE : (uint16_t)remain_len;
        if (App_Flash_ReadBuffer(read_addr, verify_buffer, read_len) != HAL_OK) {
            return false;
        }
        crc32 = App_Flash_Crc32Update(crc32, verify_buffer, read_len);
        read_addr += read_len;
        remain_len -= read_len;
    }

    crc32 ^= 0xFFFFFFFFUL;
    return (crc32 == s_flash_ctrl.image_crc32);
}

static bool App_Flash_WriteImageHeader(void)
{
    App_Flash_ImageHeader_TypeDef header;

    memset(&header, 0xFF, sizeof(header));
    header.magic = APP_FLASH_IMAGE_HEADER_MAGIC;
    header.image_size = s_flash_ctrl.image_size;
    header.image_crc32 = s_flash_ctrl.image_crc32;
    memset(header.reserved, 0xFF, sizeof(header.reserved));
    header.checksum = Crc16Compute((const uint8_t *)&header, sizeof(header) - 2U);

    return (App_Flash_WriteBuffer(APP_FLASH_APP1_BASE_ADDR, (const uint8_t *)&header, sizeof(header)) == HAL_OK);
}

static bool App_Flash_WriteBootFlag(Bootloader_Flag_EnumDef boot_flag)
{
    uint8_t boot_flag_buffer[APP_FLASH_BOOT_FLAG_SIZE];

    if (Drv_GD25Q32_EraseSector(&GD25Q32_Dev, APP_FLASH_BOOT_FLAG_ADDR) != HAL_OK) {
        return false;
    }
    osDelay(200);

    App_Flash_WriteU64LE(boot_flag_buffer, boot_flag);

    return (App_Flash_WriteBuffer(APP_FLASH_BOOT_FLAG_ADDR, boot_flag_buffer, APP_FLASH_BOOT_FLAG_SIZE) == HAL_OK);
}

static bool App_Flash_HandleResult(const App_Flash_Packet_TypeDef *packet)
{
    uint8_t reply_state;
    (void)packet;

    if (s_flash_ctrl.state != APP_FLASH_STATE_RECEIVING) {
        reply_state = 0x03U;
        App_Flash_SetReply(E_CMD_OTA_RESULT, &reply_state, 1U);
        return false;
    }

    if (s_flash_ctrl.expected_offset != s_flash_ctrl.image_size) {
        reply_state = 0x01U;
        App_Flash_SetReply(E_CMD_OTA_RESULT, &reply_state, 1U);
        return false;
    }

    s_flash_ctrl.state = APP_FLASH_STATE_VERIFYING;
    if (App_Flash_VerifyImage() == false) {
        s_flash_ctrl.state = APP_FLASH_STATE_ERROR;
        reply_state = 0x03U;
        App_Flash_SetReply(E_CMD_OTA_RESULT, &reply_state, 1U);
        return false;
    }

    if ((App_Flash_WriteImageHeader() == false) ||
        (App_Flash_WriteBootFlag(BOOTLADER_FLAG_UPDATE) == false)) {
        s_flash_ctrl.state = APP_FLASH_STATE_ERROR;
        reply_state = 0x03U;
        App_Flash_SetReply(E_CMD_OTA_RESULT, &reply_state, 1U);
        return false;
    }

    s_flash_ctrl.state = APP_FLASH_STATE_WAIT_REBOOT;
    s_flash_ctrl.reboot_armed = 1U;
    s_flash_ctrl.reboot_delay_ms = APP_FLASH_REBOOT_DELAY_MS;
    reply_state = 0x00U;
    App_Flash_SetReply(E_CMD_OTA_RESULT, &reply_state, 1U);
    return true;
}

void App_Flash_Init(void)
{
    App_Flash_ResetSession();
}

void App_Flash_Handle(void)
{
    App_Flash_Packet_TypeDef packet;

    if (osMessageQueueGet(OTA_FlashHandle, &packet, NULL, 0) == osOK) {
        switch (packet.cmd) {
            case E_CMD_OTA_REQUEST:
                App_Flash_HandleRequest(&packet);
                break;
            case E_CMD_OTA_FILE_INFO:
                App_Flash_HandleFileInfo(&packet);
                break;
            case E_CMD_OTA_OFFSET:
                App_Flash_HandleOffset(&packet);
                break;
            case E_CMD_OTA_DATA:
                App_Flash_HandleData(&packet);
                break;
            case E_CMD_OTA_RESULT:
                App_Flash_HandleResult(&packet);
                break;
            default:
                break;
        }
    }

    if ((s_flash_ctrl.state == APP_FLASH_STATE_WAIT_REBOOT) && (s_flash_ctrl.reboot_armed == 1U) && (s_flash_ctrl.reply_pending == 0U)) {
        if (s_flash_ctrl.reboot_delay_ms > APP_FLASH_HANDLE_PERIOD_MS) {
            s_flash_ctrl.reboot_delay_ms -= APP_FLASH_HANDLE_PERIOD_MS;
        } else {
            LOG_I("OTA image verified, reset to boot\r\n");
            osDelay(100);
            NVIC_SystemReset();
        }
    }
}

bool App_Flash_PushPacket(uint8_t cmd, const uint8_t *data, uint16_t len)
{
    App_Flash_Packet_TypeDef packet;

    if (len > APP_FLASH_PACKET_DATA_MAX) {
        return false;
    }

    memset(&packet, 0, sizeof(packet));
    packet.cmd = cmd;
    packet.len = len;
    if ((data != NULL) && (len > 0U)) {
        memcpy(packet.data, data, len);
    }

    return (osMessageQueuePut(OTA_FlashHandle, &packet, 0, 0) == osOK);
}

bool App_Flash_FetchReply(App_Flash_Reply_TypeDef *reply)
{
    if ((reply == NULL) || (s_flash_ctrl.reply_pending == 0U)) {
        return false;
    }

    memcpy(reply, &s_flash_ctrl.reply, sizeof(*reply));
    return true;
}

void App_Flash_NotifyReplySent(void)
{
    s_flash_ctrl.reply_pending = 0U;
}

uint32_t App_Flash_GetExpectedOffset(void)
{
    return s_flash_ctrl.expected_offset;
}

uint16_t App_Flash_GetMaxPacketLength(void)
{
    return APP_FLASH_OTA_PACKET_MAX;
}

App_Flash_State_EnumDef App_Flash_GetState(void)
{
    return s_flash_ctrl.state;
}
