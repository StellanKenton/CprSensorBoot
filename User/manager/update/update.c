/***********************************************************************************
* @file     : update.c
* @brief    : Update manager implementation.
* @details  : Implements the boot-side dual-image update state machine.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update.h"

#include <stddef.h>
#include <string.h>

#include "drv_wdg.h"
#include "gd25qxxx.h"
#include "log.h"
#include "main.h"
#include "drvmcuflash.h"
#include "drvmcuflash_port.h"

#define UPDATE_LOG_TAG                   "Update"
#define UPDATE_IO_CHUNK_SIZE             1024U
#define UPDATE_MCU_ERASE_STEP_SIZE       4096U
#define UPDATE_PROGRESS_LOG_STEP         (32UL * 1024UL)

#define UPDATE_APP_VECTOR_STACK_MASK     0x2FFE0000UL
#define UPDATE_APP_VECTOR_STACK_BASE     0x20000000UL
#define UPDATE_APP_VECTOR_ENTRY_MASK     0xFFFFFFFEUL
#define UPDATE_NVIC_REGISTER_COUNT       8U

typedef void (*updateAppEntryFunc)(void);

typedef struct stUpdateContext {
    stUpdateBootRecord bootRecord;
    stUpdateImageHeader app1Header;
    stUpdateImageHeader app2Header;
    uint32_t expectedCrc32;
    uint32_t sourceCrc32;
    uint32_t lastProgressLogOffset;
    bool isDriverReady;
    bool isApp1Verified;
} stUpdateContext;

static stUpdateStatus gUpdateStatus;
static stUpdateContext gUpdateContext;
static uint8_t gUpdateIoBuffer[UPDATE_IO_CHUNK_SIZE];

static const char *updateGetStateString(eUpdateState state);
static void updateSetState(eUpdateState state, uint32_t totalSize, uint32_t expectedCrc32);
static void updateLogProgress(const char *stageName);
static void updateFeedWatchdog(void);
static uint32_t updateCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length);
static uint32_t updateCrc32Finalize(uint32_t crc);
static bool updateReadExternal(uint32_t address, uint8_t *buffer, uint32_t length);
static bool updateWriteExternal(uint32_t address, const uint8_t *buffer, uint32_t length);
static bool updateEraseExternalRange(uint32_t baseAddress, uint32_t imageSize);
static bool updateReadImageHeader(uint32_t baseAddress, stUpdateImageHeader *header);
static bool updateWriteImageHeader(uint32_t baseAddress, const stUpdateImageHeader *header);
static bool updateWriteBootRecordInternal(const stUpdateBootRecord *record);
static void updateApplyTestBootRecordOverride(void);
static bool updateIsValidBootFlag(uint32_t requestFlag);
static bool updateIsValidImageHeader(const stUpdateImageHeader *header, uint32_t maxSize, bool requireReady);
static bool updateReadMcuApp(uint32_t offset, uint8_t *buffer, uint32_t length);
static bool updateWriteMcuApp(uint32_t offset, const uint8_t *buffer, uint32_t length);
static bool updateEraseMcuAppRange(uint32_t offset, uint32_t length);
static bool updateIsNormalAppBootFlagValue(uint32_t requestFlag);
static bool updateHasValidAppVector(void);
static void updatePrepareAppJump(uint32_t vectorBase);
static __asm void updateLaunchApp(uint32_t stackPointer, uint32_t resetHandler);
static void updateJumpToApp(void);
static bool updateShouldRollback(void);
static void updateHandleFailure(eUpdateError error);
static void updateHandleCheckRequest(void);
static void updateHandleValidateApp2(void);
static void updateHandlePrepareApp1(void);
static void updateHandleBackupAppToApp1(void);
static void updateHandleVerifyApp1(void);
static void updateHandleEraseMcuApp(void);
static void updateHandleProgramApp2ToMcu(void);
static void updateHandleVerifyMcuApp(void);
static void updateHandleClearFlag(void);
static void updateHandleRollbackEraseMcuApp(void);
static void updateHandleRollbackApp1ToMcu(void);
static void updateHandleVerifyRollback(void);

static const char *updateGetStateString(eUpdateState state)
{
    switch (state) {
        case E_UPDATE_STATE_UNINIT:
            return "UNINIT";
        case E_UPDATE_STATE_IDLE:
            return "IDLE";
        case E_UPDATE_STATE_CHECK_REQUEST:
            return "CHECK_REQUEST";
        case E_UPDATE_STATE_VALIDATE_APP2:
            return "VALIDATE_APP2";
        case E_UPDATE_STATE_PREPARE_APP1:
            return "PREPARE_APP1";
        case E_UPDATE_STATE_BACKUP_APP_TO_APP1:
            return "BACKUP_APP_TO_APP1";
        case E_UPDATE_STATE_VERIFY_APP1:
            return "VERIFY_APP1";
        case E_UPDATE_STATE_ERASE_MCU_APP:
            return "ERASE_MCU_APP";
        case E_UPDATE_STATE_PROGRAM_APP2_TO_MCU:
            return "PROGRAM_APP2_TO_MCU";
        case E_UPDATE_STATE_VERIFY_MCU_APP:
            return "VERIFY_MCU_APP";
        case E_UPDATE_STATE_CLEAR_FLAG:
            return "CLEAR_FLAG";
        case E_UPDATE_STATE_ROLLBACK_ERASE_MCU_APP:
            return "ROLLBACK_ERASE_MCU_APP";
        case E_UPDATE_STATE_ROLLBACK_APP1_TO_MCU:
            return "ROLLBACK_APP1_TO_MCU";
        case E_UPDATE_STATE_VERIFY_ROLLBACK:
            return "VERIFY_ROLLBACK";
        case E_UPDATE_STATE_JUMP_TO_APP:
            return "JUMP_TO_APP";
        case E_UPDATE_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static void updateSetState(eUpdateState state, uint32_t totalSize, uint32_t expectedCrc32)
{
    if (gUpdateStatus.state != state) {
        LOG_I(UPDATE_LOG_TAG,
              "state: %s -> %s",
              updateGetStateString(gUpdateStatus.state),
              updateGetStateString(state));
    }

    gUpdateStatus.state = state;
    gUpdateStatus.currentOffset = 0U;
    gUpdateStatus.totalSize = totalSize;
    gUpdateStatus.activeCrc32 = (totalSize > 0U) ? UPDATE_CRC32_INIT_VALUE : 0U;
    gUpdateContext.expectedCrc32 = expectedCrc32;
    gUpdateContext.lastProgressLogOffset = 0U;
}

static void updateLogProgress(const char *stageName)
{
    if ((stageName == NULL) || (gUpdateStatus.totalSize == 0U)) {
        return;
    }

    if ((gUpdateStatus.currentOffset == gUpdateStatus.totalSize) ||
        ((gUpdateStatus.currentOffset - gUpdateContext.lastProgressLogOffset) >= UPDATE_PROGRESS_LOG_STEP)) {
        gUpdateContext.lastProgressLogOffset = gUpdateStatus.currentOffset;
        LOG_I(UPDATE_LOG_TAG,
              "%s: %lu / %lu bytes",
              stageName,
              (unsigned long)gUpdateStatus.currentOffset,
              (unsigned long)gUpdateStatus.totalSize);
    }
}

static void updateFeedWatchdog(void)
{
    Drv_WatchDogFeed();
}

static uint32_t updateCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t lIndex;
    uint8_t lBitIndex;

    if ((data == NULL) && (length > 0U)) {
        return crc;
    }

    for (lIndex = 0U; lIndex < length; lIndex++) {
        crc ^= data[lIndex];
        for (lBitIndex = 0U; lBitIndex < 8U; lBitIndex++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static uint32_t updateCrc32Finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

static bool updateReadExternal(uint32_t address, uint8_t *buffer, uint32_t length)
{
    if (gd25qxxxRead(GD25Q32_MEM, address, buffer, length) != GD25QXXX_STATUS_OK) {
        LOG_E(UPDATE_LOG_TAG,
              "ext flash read failed, addr=0x%08lX, len=%lu",
              (unsigned long)address,
              (unsigned long)length);
        return false;
    }

    return true;
}

static bool updateWriteExternal(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    if (gd25qxxxWrite(GD25Q32_MEM, address, buffer, length) != GD25QXXX_STATUS_OK) {
        LOG_E(UPDATE_LOG_TAG,
              "ext flash write failed, addr=0x%08lX, len=%lu",
              (unsigned long)address,
              (unsigned long)length);
        return false;
    }

    return true;
}

static bool updateEraseExternalRange(uint32_t baseAddress, uint32_t imageSize)
{
    uint32_t lEraseEndAddress;
    uint32_t lCurrentAddress;
    uint32_t lRemainingLength;
    eGd25qxxxStatus lStatus;

    lEraseEndAddress = baseAddress + APP_FLASH_APP_CRC_RESERVED_SIZE + imageSize;
    lEraseEndAddress = (lEraseEndAddress + APP_FLASH_SECTOR_SIZE - 1U) & ~(APP_FLASH_SECTOR_SIZE - 1U);
    lCurrentAddress = baseAddress;

    while (lCurrentAddress < lEraseEndAddress) {
        lRemainingLength = lEraseEndAddress - lCurrentAddress;
        if (((lCurrentAddress % GD25QXXX_BLOCK64K_SIZE) == 0U) && (lRemainingLength >= GD25QXXX_BLOCK64K_SIZE)) {
            lStatus = gd25qxxxEraseBlock64k(GD25Q32_MEM, lCurrentAddress);
            lCurrentAddress += GD25QXXX_BLOCK64K_SIZE;
        } else {
            lStatus = gd25qxxxEraseSector(GD25Q32_MEM, lCurrentAddress);
            lCurrentAddress += GD25QXXX_SECTOR_SIZE;
        }

        if (lStatus != GD25QXXX_STATUS_OK) {
            LOG_E(UPDATE_LOG_TAG,
                  "ext flash erase failed, addr=0x%08lX, status=%d",
                  (unsigned long)(lCurrentAddress - APP_FLASH_SECTOR_SIZE),
                  (int)lStatus);
            return false;
        }

        updateFeedWatchdog();
    }

    return true;
}

static bool updateReadImageHeader(uint32_t baseAddress, stUpdateImageHeader *header)
{
    if (header == NULL) {
        return false;
    }

    return updateReadExternal(baseAddress, (uint8_t *)header, sizeof(stUpdateImageHeader));
}

static bool updateWriteImageHeader(uint32_t baseAddress, const stUpdateImageHeader *header)
{
    if (header == NULL) {
        return false;
    }

    if (gd25qxxxEraseSector(GD25Q32_MEM, baseAddress) != GD25QXXX_STATUS_OK) {
        LOG_E(UPDATE_LOG_TAG, "image header erase failed, addr=0x%08lX", (unsigned long)baseAddress);
        return false;
    }

    return updateWriteExternal(baseAddress, (const uint8_t *)header, sizeof(stUpdateImageHeader));
}

static bool updateWriteBootRecordInternal(const stUpdateBootRecord *record)
{
    if (record == NULL) {
        return false;
    }

    if (gd25qxxxEraseSector(GD25Q32_MEM, APP_FLASH_BOOT_FLAG_ADDR) != GD25QXXX_STATUS_OK) {
        LOG_E(UPDATE_LOG_TAG, "boot record erase failed");
        return false;
    }

    return updateWriteExternal(APP_FLASH_BOOT_FLAG_ADDR, (const uint8_t *)record, sizeof(stUpdateBootRecord));
}

static void updateApplyTestBootRecordOverride(void)
{
#if (UPDATE_TEST_FORCE_APP_REQUEST_ENABLE == 1U)
    if ((gUpdateContext.bootRecord.magic != UPDATE_BOOT_RECORD_MAGIC) ||
        (gUpdateContext.bootRecord.requestFlag != (uint32_t)E_UPDATE_BOOT_FLAG_IDLE)) {
        return;
    }

    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_BOOT_FLAG_APP_REQUEST;
    gUpdateContext.bootRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;

    if (!updateWriteBootRecordInternal(&gUpdateContext.bootRecord)) {
        LOG_E(UPDATE_LOG_TAG, "test override boot record failed");
        return;
    }

    LOG_W(UPDATE_LOG_TAG, "test override boot flag: IDLE -> APP_REQUEST");
#endif
}

static bool updateIsValidBootFlag(uint32_t requestFlag)
{
    return (requestFlag <= (uint32_t)E_UPDATE_BOOT_FLAG_FAILED);
}

static bool updateIsNormalAppBootFlagValue(uint32_t requestFlag)
{
    return (requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_IDLE) ||
           (requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_SUCCESS) ||
           (requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_FAILED);
}

static bool updateIsValidImageHeader(const stUpdateImageHeader *header, uint32_t maxSize, bool requireReady)
{
    if (header == NULL) {
        return false;
    }

    if ((header->magic != UPDATE_IMAGE_MAGIC) ||
        (header->headerVersion != UPDATE_HEADER_VERSION) ||
        (header->imageSize == 0U) ||
        (header->imageSize > maxSize) ||
        (header->imageSize > UPDATE_MCU_APP_SIZE)) {
        return false;
    }

    if (requireReady) {
        if ((header->imageState != (uint32_t)E_UPDATE_IMAGE_STATE_READY) ||
            (header->writeOffset < header->imageSize)) {
            return false;
        }
    }

    return true;
}

static bool updateReadMcuApp(uint32_t offset, uint8_t *buffer, uint32_t length)
{
    return drvMcuFlashRead(DRVMCUFLASH_AREA_APP, offset, buffer, length) == DRV_STATUS_OK;
}

static bool updateWriteMcuApp(uint32_t offset, const uint8_t *buffer, uint32_t length)
{
    return drvMcuFlashWrite(DRVMCUFLASH_AREA_APP, offset, buffer, length) == DRV_STATUS_OK;
}

static bool updateEraseMcuAppRange(uint32_t offset, uint32_t length)
{
    return drvMcuFlashErase(DRVMCUFLASH_AREA_APP, offset, length) == DRV_STATUS_OK;
}

static bool updateHasValidAppVector(void)
{
    uint32_t lVector[2] = {0U, 0U};
    uint32_t lResetHandler;

    if (drvMcuFlashRead(DRVMCUFLASH_AREA_APP, 0U, (uint8_t *)lVector, sizeof(lVector)) != DRV_STATUS_OK) {
        return false;
    }

    if ((lVector[0] & UPDATE_APP_VECTOR_STACK_MASK) != UPDATE_APP_VECTOR_STACK_BASE) {
        return false;
    }

    if ((lVector[1] & 0x1U) == 0U) {
        return false;
    }

    lResetHandler = lVector[1] & UPDATE_APP_VECTOR_ENTRY_MASK;
    return (lResetHandler >= UPDATE_MCU_APP_START_ADDR) &&
           (lResetHandler < (UPDATE_MCU_APP_START_ADDR + UPDATE_MCU_APP_SIZE));
}

static void updatePrepareAppJump(uint32_t vectorBase)
{
    uint32_t lIndex;

    __disable_irq();
    HAL_DeInit();
    HAL_RCC_DeInit();
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (lIndex = 0U; lIndex < UPDATE_NVIC_REGISTER_COUNT; lIndex++) {
        NVIC->ICER[lIndex] = 0xFFFFFFFFUL;
        NVIC->ICPR[lIndex] = 0xFFFFFFFFUL;
    }

    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk);
    SCB->CFSR = 0xFFFFFFFFUL;
    SCB->HFSR = 0xFFFFFFFFUL;
    SCB->DFSR = 0xFFFFFFFFUL;
    SCB->AFSR = 0xFFFFFFFFUL;

    __set_CONTROL(0U);
    __set_PSP(0U);
    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);
    SCB->VTOR = vectorBase;

    __DSB();
    __ISB();
}

static __asm void updateLaunchApp(uint32_t stackPointer, uint32_t resetHandler)
{
    MSR MSP, r0
    DSB
    ISB
    BX  r1
}

static void updateJumpToApp(void)
{
    uint32_t lVector[2] = {0U, 0U};

    if (drvMcuFlashRead(DRVMCUFLASH_AREA_APP, 0U, (uint8_t *)lVector, sizeof(lVector)) != DRV_STATUS_OK) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_APP_VECTOR_INVALID;
        gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        return;
    }

    if (!updateHasValidAppVector()) {
        LOG_E(UPDATE_LOG_TAG, "invalid app vector table");
        gUpdateStatus.lastError = E_UPDATE_ERROR_APP_VECTOR_INVALID;
        gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        return;
    }

    LOG_I(UPDATE_LOG_TAG, "jump to app: sp=0x%08lX, reset=0x%08lX", (unsigned long)lVector[0], (unsigned long)lVector[1]);
    updatePrepareAppJump(UPDATE_MCU_APP_START_ADDR);
    updateLaunchApp(lVector[0], lVector[1]);

    gUpdateStatus.lastError = E_UPDATE_ERROR_APP_VECTOR_INVALID;
    gUpdateStatus.state = E_UPDATE_STATE_ERROR;
}

static bool updateShouldRollback(void)
{
    switch (gUpdateStatus.state) {
        case E_UPDATE_STATE_ERASE_MCU_APP:
        case E_UPDATE_STATE_PROGRAM_APP2_TO_MCU:
        case E_UPDATE_STATE_VERIFY_MCU_APP:
        case E_UPDATE_STATE_CLEAR_FLAG:
        case E_UPDATE_STATE_ROLLBACK_ERASE_MCU_APP:
        case E_UPDATE_STATE_ROLLBACK_APP1_TO_MCU:
        case E_UPDATE_STATE_VERIFY_ROLLBACK:
            return true;
        default:
            return false;
    }
}

static void updateHandleFailure(eUpdateError error)
{
    gUpdateStatus.lastError = error;
    gUpdateContext.bootRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    gUpdateContext.bootRecord.lastError = (uint32_t)error;
    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_BOOT_FLAG_FAILED;
    gUpdateStatus.requestFlag = E_UPDATE_BOOT_FLAG_FAILED;

    if (gUpdateContext.isDriverReady) {
        (void)updateWriteBootRecordInternal(&gUpdateContext.bootRecord);
    }

    LOG_E(UPDATE_LOG_TAG, "update failed, error=%d, state=%s", (int)error, updateGetStateString(gUpdateStatus.state));

    if (updateShouldRollback() && gUpdateContext.isApp1Verified) {
        gUpdateStatus.isRollbackActive = true;
        updateSetState(E_UPDATE_STATE_ROLLBACK_ERASE_MCU_APP, gUpdateContext.app1Header.imageSize, 0U);
        return;
    }

    if (updateHasValidAppVector()) {
        updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
        return;
    }

    gUpdateStatus.state = E_UPDATE_STATE_ERROR;
}

static void updateHandleCheckRequest(void)
{
    if (!gUpdateContext.isDriverReady) {
        if (updateHasValidAppVector()) {
            updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
        } else {
            gUpdateStatus.lastError = E_UPDATE_ERROR_FLASH_ACCESS_FAILED;
            gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        }
        return;
    }

    if (!updateGetBootRecord(&gUpdateContext.bootRecord)) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_BOOT_RECORD_INVALID;
        if (updateHasValidAppVector()) {
            updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
        } else {
            gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        }
        return;
    }

    updateApplyTestBootRecordOverride();

    if ((gUpdateContext.bootRecord.magic != UPDATE_BOOT_RECORD_MAGIC) ||
        !updateIsValidBootFlag(gUpdateContext.bootRecord.requestFlag) ||
        (gUpdateContext.bootRecord.requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_IDLE) ||
        (gUpdateContext.bootRecord.requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_SUCCESS) ||
        (gUpdateContext.bootRecord.requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_FAILED)) {
        gUpdateStatus.isUpdateRequested = false;
        gUpdateStatus.requestFlag = E_UPDATE_BOOT_FLAG_IDLE;
        updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
        return;
    }

    if (!updateReadImageHeader(APP_FLASH_APP2_BASE_ADDR, &gUpdateContext.app2Header) ||
        !updateIsValidImageHeader(&gUpdateContext.app2Header, APP_FLASH_APP2_DATA_MAX_SIZE, true)) {
        updateHandleFailure(E_UPDATE_ERROR_APP2_HEADER_INVALID);
        return;
    }

    gUpdateStatus.isUpdateRequested = true;
    gUpdateStatus.requestFlag = (eUpdateBootFlag)gUpdateContext.bootRecord.requestFlag;
    gUpdateContext.bootRecord.appSize = gUpdateContext.app2Header.imageSize;
    gUpdateContext.bootRecord.app2Crc32 = gUpdateContext.app2Header.imageCrc32;

    if ((gUpdateContext.bootRecord.requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_BACKUP_DONE) ||
        (gUpdateContext.bootRecord.requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_PROGRAM_DONE)) {
        if (!updateReadImageHeader(APP_FLASH_APP1_BASE_ADDR, &gUpdateContext.app1Header) ||
            !updateIsValidImageHeader(&gUpdateContext.app1Header, APP_FLASH_APP1_DATA_MAX_SIZE, true)) {
            updateHandleFailure(E_UPDATE_ERROR_APP1_CRC_MISMATCH);
            return;
        }

        gUpdateContext.isApp1Verified = true;
        gUpdateContext.sourceCrc32 = gUpdateContext.app1Header.imageCrc32;
        gUpdateContext.bootRecord.app1Crc32 = gUpdateContext.app1Header.imageCrc32;
    }

    if (gUpdateContext.bootRecord.requestFlag == (uint32_t)E_UPDATE_BOOT_FLAG_PROGRAM_DONE) {
        updateSetState(E_UPDATE_STATE_VERIFY_MCU_APP,
                       gUpdateContext.app2Header.imageSize,
                       gUpdateContext.app2Header.imageCrc32);
        return;
    }

    updateSetState(E_UPDATE_STATE_VALIDATE_APP2,
                   gUpdateContext.app2Header.imageSize,
                   gUpdateContext.app2Header.imageCrc32);
}

static void updateHandleValidateApp2(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_IO_CHUNK_SIZE) {
        lChunkSize = UPDATE_IO_CHUNK_SIZE;
    }

    if (!updateReadExternal(APP_FLASH_APP2_DATA_ADDR + gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_FLASH_ACCESS_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress("verify app2");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Finalize(gUpdateStatus.activeCrc32);
    if (gUpdateStatus.activeCrc32 != gUpdateContext.expectedCrc32) {
        updateHandleFailure(E_UPDATE_ERROR_APP2_CRC_MISMATCH);
        return;
    }

    LOG_I(UPDATE_LOG_TAG, "app2 crc verified: 0x%08lX", (unsigned long)gUpdateStatus.activeCrc32);
    if (gUpdateContext.isApp1Verified) {
        updateSetState(E_UPDATE_STATE_ERASE_MCU_APP, UPDATE_MCU_APP_SIZE, 0U);
    } else {
        updateSetState(E_UPDATE_STATE_PREPARE_APP1, UPDATE_MCU_APP_SIZE, 0U);
    }
}

static void updateHandlePrepareApp1(void)
{
    (void)memset(&gUpdateContext.app1Header, 0xFF, sizeof(gUpdateContext.app1Header));
    gUpdateContext.app1Header.magic = UPDATE_IMAGE_MAGIC;
    gUpdateContext.app1Header.headerVersion = UPDATE_HEADER_VERSION;
    gUpdateContext.app1Header.imageSize = UPDATE_MCU_APP_SIZE;
    gUpdateContext.app1Header.writeOffset = 0U;
    gUpdateContext.app1Header.imageState = (uint32_t)E_UPDATE_IMAGE_STATE_RECEIVING;

    if (!updateEraseExternalRange(APP_FLASH_APP1_BASE_ADDR, UPDATE_MCU_APP_SIZE) ||
        !updateWriteImageHeader(APP_FLASH_APP1_BASE_ADDR, &gUpdateContext.app1Header)) {
        updateHandleFailure(E_UPDATE_ERROR_APP1_BACKUP_WRITE_FAILED);
        return;
    }

    gUpdateContext.sourceCrc32 = UPDATE_CRC32_INIT_VALUE;
    updateSetState(E_UPDATE_STATE_BACKUP_APP_TO_APP1, UPDATE_MCU_APP_SIZE, 0U);
}

static void updateHandleBackupAppToApp1(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_IO_CHUNK_SIZE) {
        lChunkSize = UPDATE_IO_CHUNK_SIZE;
    }

    if (!updateReadMcuApp(gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize) ||
        !updateWriteExternal(APP_FLASH_APP1_DATA_ADDR + gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_APP1_BACKUP_WRITE_FAILED);
        return;
    }

    gUpdateContext.sourceCrc32 = updateCrc32Update(gUpdateContext.sourceCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress("backup app1");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateContext.sourceCrc32 = updateCrc32Finalize(gUpdateContext.sourceCrc32);
    gUpdateContext.app1Header.imageCrc32 = gUpdateContext.sourceCrc32;
    gUpdateContext.app1Header.writeOffset = gUpdateStatus.totalSize;
    gUpdateContext.app1Header.imageState = (uint32_t)E_UPDATE_IMAGE_STATE_RECEIVING;

    if (!updateWriteImageHeader(APP_FLASH_APP1_BASE_ADDR, &gUpdateContext.app1Header)) {
        updateHandleFailure(E_UPDATE_ERROR_APP1_BACKUP_WRITE_FAILED);
        return;
    }

    updateSetState(E_UPDATE_STATE_VERIFY_APP1, UPDATE_MCU_APP_SIZE, gUpdateContext.sourceCrc32);
}

static void updateHandleVerifyApp1(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_IO_CHUNK_SIZE) {
        lChunkSize = UPDATE_IO_CHUNK_SIZE;
    }

    if (!updateReadExternal(APP_FLASH_APP1_DATA_ADDR + gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_FLASH_ACCESS_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress("verify app1");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Finalize(gUpdateStatus.activeCrc32);
    if (gUpdateStatus.activeCrc32 != gUpdateContext.expectedCrc32) {
        updateHandleFailure(E_UPDATE_ERROR_APP1_CRC_MISMATCH);
        return;
    }

    gUpdateContext.isApp1Verified = true;
    gUpdateContext.bootRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_BOOT_FLAG_BACKUP_DONE;
    gUpdateContext.bootRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    gUpdateContext.bootRecord.app1Crc32 = gUpdateStatus.activeCrc32;
    gUpdateContext.bootRecord.app2Crc32 = gUpdateContext.app2Header.imageCrc32;
    gUpdateContext.bootRecord.appSize = gUpdateContext.app2Header.imageSize;
    gUpdateStatus.requestFlag = E_UPDATE_BOOT_FLAG_BACKUP_DONE;

    gUpdateContext.app1Header.imageCrc32 = gUpdateStatus.activeCrc32;
    gUpdateContext.app1Header.writeOffset = gUpdateContext.app1Header.imageSize;
    gUpdateContext.app1Header.imageState = (uint32_t)E_UPDATE_IMAGE_STATE_READY;

    if (!updateWriteBootRecordInternal(&gUpdateContext.bootRecord) ||
        !updateWriteImageHeader(APP_FLASH_APP1_BASE_ADDR, &gUpdateContext.app1Header)) {
        updateHandleFailure(E_UPDATE_ERROR_APP1_BACKUP_WRITE_FAILED);
        return;
    }

    LOG_I(UPDATE_LOG_TAG, "app1 backup verified: 0x%08lX", (unsigned long)gUpdateStatus.activeCrc32);
    updateSetState(E_UPDATE_STATE_ERASE_MCU_APP, UPDATE_MCU_APP_SIZE, 0U);
}

static void updateHandleEraseMcuApp(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_MCU_ERASE_STEP_SIZE) {
        lChunkSize = UPDATE_MCU_ERASE_STEP_SIZE;
    }

    if (!updateEraseMcuAppRange(gUpdateStatus.currentOffset, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_MCU_APP_ERASE_FAILED);
        return;
    }

    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress(gUpdateStatus.isRollbackActive ? "rollback erase mcu" : "erase mcu");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    if (gUpdateStatus.isRollbackActive) {
        updateSetState(E_UPDATE_STATE_ROLLBACK_APP1_TO_MCU,
                       gUpdateContext.app1Header.imageSize,
                       0U);
    } else {
        updateSetState(E_UPDATE_STATE_PROGRAM_APP2_TO_MCU,
                       gUpdateContext.app2Header.imageSize,
                       0U);
    }
}

static void updateHandleProgramApp2ToMcu(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_IO_CHUNK_SIZE) {
        lChunkSize = UPDATE_IO_CHUNK_SIZE;
    }

    if (!updateReadExternal(APP_FLASH_APP2_DATA_ADDR + gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize) ||
        !updateWriteMcuApp(gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_MCU_APP_PROGRAM_FAILED);
        return;
    }

    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress("program mcu");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateContext.bootRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_BOOT_FLAG_PROGRAM_DONE;
    gUpdateContext.bootRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    gUpdateContext.bootRecord.app1Crc32 = gUpdateContext.app1Header.imageCrc32;
    gUpdateContext.bootRecord.app2Crc32 = gUpdateContext.app2Header.imageCrc32;
    gUpdateContext.bootRecord.appSize = gUpdateContext.app2Header.imageSize;
    gUpdateStatus.requestFlag = E_UPDATE_BOOT_FLAG_PROGRAM_DONE;

    if (!updateWriteBootRecordInternal(&gUpdateContext.bootRecord)) {
        updateHandleFailure(E_UPDATE_ERROR_FLASH_ACCESS_FAILED);
        return;
    }

    updateSetState(E_UPDATE_STATE_VERIFY_MCU_APP,
                   gUpdateContext.app2Header.imageSize,
                   gUpdateContext.app2Header.imageCrc32);
}

static void updateHandleVerifyMcuApp(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_IO_CHUNK_SIZE) {
        lChunkSize = UPDATE_IO_CHUNK_SIZE;
    }

    if (!updateReadMcuApp(gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_MCU_FLASH_ACCESS_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress("verify mcu");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Finalize(gUpdateStatus.activeCrc32);
    if (gUpdateStatus.activeCrc32 != gUpdateContext.expectedCrc32) {
        updateHandleFailure(E_UPDATE_ERROR_MCU_APP_CRC_MISMATCH);
        return;
    }

    LOG_I(UPDATE_LOG_TAG, "mcu app verified: 0x%08lX", (unsigned long)gUpdateStatus.activeCrc32);
    updateSetState(E_UPDATE_STATE_CLEAR_FLAG, 0U, 0U);
}

static void updateHandleClearFlag(void)
{
    gUpdateContext.bootRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_BOOT_FLAG_IDLE;
    gUpdateContext.bootRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    gUpdateContext.bootRecord.app1Crc32 = gUpdateContext.app1Header.imageCrc32;
    gUpdateContext.bootRecord.app2Crc32 = gUpdateContext.app2Header.imageCrc32;
    gUpdateContext.bootRecord.appSize = gUpdateContext.app2Header.imageSize;

    gUpdateStatus.requestFlag = E_UPDATE_BOOT_FLAG_IDLE;
    gUpdateStatus.lastError = E_UPDATE_ERROR_NONE;
    gUpdateStatus.isRollbackActive = false;

    if (!updateWriteBootRecordInternal(&gUpdateContext.bootRecord)) {
        LOG_W(UPDATE_LOG_TAG, "clear update flag failed, jump app with verified image");
    }

    updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
}

static void updateHandleRollbackEraseMcuApp(void)
{
    if (!gUpdateContext.isApp1Verified) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_ROLLBACK_FAILED;
        gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        return;
    }

    updateHandleEraseMcuApp();
}

static void updateHandleRollbackApp1ToMcu(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_IO_CHUNK_SIZE) {
        lChunkSize = UPDATE_IO_CHUNK_SIZE;
    }

    if (!updateReadExternal(APP_FLASH_APP1_DATA_ADDR + gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize) ||
        !updateWriteMcuApp(gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_ROLLBACK_FAILED;
        gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        return;
    }

    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress("rollback");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    updateSetState(E_UPDATE_STATE_VERIFY_ROLLBACK,
                   gUpdateContext.app1Header.imageSize,
                   gUpdateContext.app1Header.imageCrc32);
}

static void updateHandleVerifyRollback(void)
{
    uint32_t lChunkSize;

    lChunkSize = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    if (lChunkSize > UPDATE_IO_CHUNK_SIZE) {
        lChunkSize = UPDATE_IO_CHUNK_SIZE;
    }

    if (!updateReadMcuApp(gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_ROLLBACK_FAILED;
        gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateFeedWatchdog();
    updateLogProgress("verify rollback");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Finalize(gUpdateStatus.activeCrc32);
    if (gUpdateStatus.activeCrc32 != gUpdateContext.expectedCrc32) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_ROLLBACK_FAILED;
        gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        return;
    }

    gUpdateContext.bootRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_BOOT_FLAG_FAILED;
    gUpdateContext.bootRecord.lastError = (uint32_t)gUpdateStatus.lastError;
    gUpdateContext.bootRecord.app1Crc32 = gUpdateContext.app1Header.imageCrc32;
    gUpdateContext.bootRecord.app2Crc32 = gUpdateContext.app2Header.imageCrc32;
    gUpdateContext.bootRecord.appSize = gUpdateContext.app2Header.imageSize;
    gUpdateStatus.requestFlag = E_UPDATE_BOOT_FLAG_FAILED;
    gUpdateStatus.isRollbackActive = false;

    (void)updateWriteBootRecordInternal(&gUpdateContext.bootRecord);
    LOG_W(UPDATE_LOG_TAG, "rollback restored previous app");
    updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
}

void updateReset(void)
{
    (void)memset(&gUpdateStatus, 0, sizeof(gUpdateStatus));
    (void)memset(&gUpdateContext, 0, sizeof(gUpdateContext));
    gUpdateStatus.state = E_UPDATE_STATE_UNINIT;
    gUpdateStatus.requestFlag = E_UPDATE_BOOT_FLAG_IDLE;
    gUpdateStatus.lastError = E_UPDATE_ERROR_NONE;
}

bool updateInit(void)
{
    updateReset();

    if (gd25qxxxInit(GD25Q32_MEM) != GD25QXXX_STATUS_OK) {
        LOG_E(UPDATE_LOG_TAG, "gd25q32 init failed");
        gUpdateStatus.lastError = E_UPDATE_ERROR_FLASH_ACCESS_FAILED;
        gUpdateContext.isDriverReady = false;
        if (updateHasValidAppVector()) {
            updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
        } else {
            gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        }
        return false;
    }

    if (drvMcuFlashInit() != DRV_STATUS_OK) {
        LOG_E(UPDATE_LOG_TAG, "mcu flash init failed");
        gUpdateStatus.lastError = E_UPDATE_ERROR_MCU_FLASH_ACCESS_FAILED;
        gUpdateContext.isDriverReady = false;
        if (updateHasValidAppVector()) {
            updateSetState(E_UPDATE_STATE_JUMP_TO_APP, 0U, 0U);
        } else {
            gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        }
        return false;
    }

    gUpdateContext.isDriverReady = true;
    updateSetState(E_UPDATE_STATE_CHECK_REQUEST, 0U, 0U);
    return true;
}


void updateProcess(uint32_t nowTick)
{
    gUpdateStatus.lastProcessTick = nowTick;

    switch (gUpdateStatus.state) {
        case E_UPDATE_STATE_UNINIT:
            return;
        case E_UPDATE_STATE_IDLE:
            updateSetState(E_UPDATE_STATE_CHECK_REQUEST, 0U, 0U);
            break;
        case E_UPDATE_STATE_CHECK_REQUEST:
            updateHandleCheckRequest();
            break;
        case E_UPDATE_STATE_VALIDATE_APP2:
            updateHandleValidateApp2();
            break;
        case E_UPDATE_STATE_PREPARE_APP1:
            updateHandlePrepareApp1();
            break;
        case E_UPDATE_STATE_BACKUP_APP_TO_APP1:
            updateHandleBackupAppToApp1();
            break;
        case E_UPDATE_STATE_VERIFY_APP1:
            updateHandleVerifyApp1();
            break;
        case E_UPDATE_STATE_ERASE_MCU_APP:
            updateHandleEraseMcuApp();
            break;
        case E_UPDATE_STATE_PROGRAM_APP2_TO_MCU:
            updateHandleProgramApp2ToMcu();
            break;
        case E_UPDATE_STATE_VERIFY_MCU_APP:
            updateHandleVerifyMcuApp();
            break;
        case E_UPDATE_STATE_CLEAR_FLAG:
            updateHandleClearFlag();
            break;
        case E_UPDATE_STATE_ROLLBACK_ERASE_MCU_APP:
            updateHandleRollbackEraseMcuApp();
            break;
        case E_UPDATE_STATE_ROLLBACK_APP1_TO_MCU:
            updateHandleRollbackApp1ToMcu();
            break;
        case E_UPDATE_STATE_VERIFY_ROLLBACK:
            updateHandleVerifyRollback();
            break;
        case E_UPDATE_STATE_JUMP_TO_APP:
            updateJumpToApp();
            break;
        case E_UPDATE_STATE_ERROR:
        default:
            break;
    }
}

const stUpdateStatus *updateGetStatus(void)
{
    return &gUpdateStatus;
}

bool updateGetBootRecord(stUpdateBootRecord *record)
{
    if (record == NULL) {
        return false;
    }

    if (!gUpdateContext.isDriverReady) {
        return false;
    }

    return updateReadExternal(APP_FLASH_BOOT_FLAG_ADDR, (uint8_t *)record, sizeof(stUpdateBootRecord));
}

bool updateHasNormalAppBootFlag(void)
{
    stUpdateBootRecord lBootRecord;

    if (!updateGetBootRecord(&lBootRecord)) {
        return false;
    }

    if (lBootRecord.magic != UPDATE_BOOT_RECORD_MAGIC) {
        return false;
    }

    return updateIsNormalAppBootFlagValue(lBootRecord.requestFlag);
}

bool updateJumpToAppIfValid(void)
{
    if (!updateHasValidAppVector()) {
        return false;
    }

    updateJumpToApp();
    return false;
}
/**************************End of file********************************/
