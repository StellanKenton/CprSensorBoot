/***********************************************************************************
* @file     : update_mgr.c
* @brief    : Update manager adapter implementation.
* @details  : Bridges project-specific watchdog and jump handling to the reusable
*             rep update service.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update_mgr.h"

#include <stddef.h>
#include <string.h>

#include "drv_wdg.h"
#include "log.h"
#include "main.h"
#include "drvmcuflash.h"

#include "../../../rep/service/update/update_port.h"

#define UPDATE_MANAGER_LOG_TAG             "UpdateMgr"
#define UPDATE_APP_VECTOR_STACK_MASK       0x2FFE0000UL
#define UPDATE_APP_VECTOR_STACK_BASE       0x20000000UL
#define UPDATE_APP_VECTOR_ENTRY_MASK       0xFFFFFFFEUL
#define UPDATE_NVIC_REGISTER_COUNT         8U

static struct {
    uint32_t idleStartTickMs;
    bool isIdleTimerActive;
    bool isIdleRecoveryHandled;
} gUpdateManagerState;

static uint8_t gUpdateManagerCopyBuffer[UPDATE_MANAGER_COPY_CHUNK_SIZE];

static bool updateManagerCanBootNormally(const stUpdateBootRecord *record);
static uint32_t updateManagerCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length);
static uint32_t updateManagerCrc32Finalize(uint32_t crc);
static uint32_t updateManagerCalcHeaderCrc(const stUpdateImageHeader *header);
static bool updateManagerGetRegionOps(uint8_t regionId, stUpdateRegionCfg *cfg, const stUpdateStorageOps **ops);
static bool updateManagerReadRegion(uint8_t regionId, uint32_t offset, uint8_t *buffer, uint32_t length);
static bool updateManagerWriteRegion(uint8_t regionId, uint32_t offset, const uint8_t *buffer, uint32_t length);
static bool updateManagerEraseRegion(uint8_t regionId, uint32_t offset, uint32_t length);
static bool updateManagerReadMetaPayload(uint8_t regionId, void *payload, uint32_t payloadSize);
static bool updateManagerIsHeaderValid(const stUpdateImageHeader *header, uint32_t maxImageSize, bool requireReady);
static bool updateManagerLoadBackupHeader(stUpdateImageHeader *header);
static bool updateManagerGetExecutableRegion(uint8_t regionId, stUpdateRegionCfg *cfg);
static bool updateManagerReadAppVector(const stUpdateRegionCfg *cfg, uint32_t vector[2]);
static bool updateManagerValidateExecutableRegion(uint8_t regionId);
static bool updateManagerVerifyRegionCrc(uint8_t regionId, uint32_t imageSize, uint32_t expectedCrc32);
static bool updateManagerRestoreAppFromBackup(void);
static void updateManagerHandleIdleTimeout(void);
static void updateManagerProcessIdleRecovery(const stUpdateStatus *status, uint32_t nowTickMs);
static bool updateManagerJumpToRegionInternal(uint8_t regionId);
static void updateManagerResetPeripheralState(void);
static void updateManagerPrepareAppJump(uint32_t vectorBase);
static __NO_RETURN __ASM void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler);

void updateManagerReset(void)
{
    updateReset();
    (void)memset(&gUpdateManagerState, 0, sizeof(gUpdateManagerState));
}

bool updateManagerInit(void)
{
    (void)memset(&gUpdateManagerState, 0, sizeof(gUpdateManagerState));

    if (!updateInit()) {
        return false;
    }

#if (UPDATE_MANAGER_FORCE_RUN_APP_BOOT_RECORD == 1)
    if (!updateManagerSetBootRecordRunApp()) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "force run_app boot record failed");
        return false;
    }
#endif

    return true;
}

void updateManagerProcess(uint32_t nowTickMs)
{
    const stUpdateStatus *lStatus;

    updateProcess(nowTickMs);

    lStatus = updateGetStatus();
    if ((lStatus != NULL) && (lStatus->state == E_UPDATE_STATE_JUMP_TARGET)) {
        gUpdateManagerState.isIdleTimerActive = false;
        gUpdateManagerState.isIdleRecoveryHandled = false;
        (void)updateJumpToTargetIfValid();
        return;
    }

    updateManagerProcessIdleRecovery(lStatus, nowTickMs);
}

const stUpdateStatus *updateManagerGetStatus(void)
{
    return updateGetStatus();
}

bool updateManagerGetBootRecord(stUpdateBootRecord *record)
{
    return updateReadBootRecord(record);
}

bool updateManagerHasNormalAppBootFlag(void)
{
    stUpdateBootRecord lRecord;

    if (!updateReadBootRecord(&lRecord)) {
        return false;
    }

    return updateManagerCanBootNormally(&lRecord);
}

bool updateManagerJumpToAppIfValid(void)
{
    return updateJumpToTargetIfValid();
}

bool updateManagerSetBootRecordRunApp(void)
{
    stUpdateBootRecord lRecord;

    if (!updateReadBootRecord(&lRecord)) {
        (void)memset(&lRecord, 0, sizeof(lRecord));
        lRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    }

    lRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    lRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_RUN_APP;
    lRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    lRecord.targetRegion = (uint32_t)E_UPDATE_REGION_RUN_APP;

    if (!updateWriteBootRecord(&lRecord)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "store run_app boot record failed");
        return false;
    }

    LOG_I(UPDATE_MANAGER_LOG_TAG, "boot record forced to run_app");
    return true;
}

static bool updateManagerCanBootNormally(const stUpdateBootRecord *record)
{
    if (record == NULL) {
        return false;
    }

    if (record->magic != UPDATE_BOOT_RECORD_MAGIC) {
        return false;
    }

    return (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_IDLE) ||
           (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_RUN_APP) ||
           (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_FAILED);
}

static uint32_t updateManagerCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t lIndex;
    uint8_t lBit;

    if ((data == NULL) && (length > 0U)) {
        return crc;
    }

    for (lIndex = 0U; lIndex < length; lIndex++) {
        crc ^= data[lIndex];
        for (lBit = 0U; lBit < 8U; lBit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static uint32_t updateManagerCrc32Finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

static uint32_t updateManagerCalcHeaderCrc(const stUpdateImageHeader *header)
{
    if (header == NULL) {
        return 0U;
    }

    return updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                               (const uint8_t *)header,
                                                               offsetof(stUpdateImageHeader, headerCrc32)));
}

static bool updateManagerGetRegionOps(uint8_t regionId, stUpdateRegionCfg *cfg, const stUpdateStorageOps **ops)
{
    stUpdateRegionCfg lCfg;
    const stUpdateStorageOps *lOps;

    if ((cfg == NULL) || (ops == NULL) || !updatePortGetRegionMap(regionId, &lCfg)) {
        return false;
    }

    lOps = updatePortGetStorageOps(lCfg.storageId);
    if ((lOps == NULL) || (lOps->read == NULL)) {
        return false;
    }

    *cfg = lCfg;
    *ops = lOps;
    return true;
}

static bool updateManagerReadRegion(uint8_t regionId, uint32_t offset, uint8_t *buffer, uint32_t length)
{
    stUpdateRegionCfg lCfg;
    const stUpdateStorageOps *lOps;
    uint32_t lAddress;

    if ((buffer == NULL) || (length == 0U) || !updateManagerGetRegionOps(regionId, &lCfg, &lOps)) {
        return false;
    }

    if (!lCfg.isReadable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lOps->isRangeValid != NULL) && !lOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lOps->read(lAddress, buffer, length);
}

static bool updateManagerWriteRegion(uint8_t regionId, uint32_t offset, const uint8_t *buffer, uint32_t length)
{
    stUpdateRegionCfg lCfg;
    const stUpdateStorageOps *lOps;
    uint32_t lAddress;

    if ((buffer == NULL) || (length == 0U) || !updateManagerGetRegionOps(regionId, &lCfg, &lOps)) {
        return false;
    }

    if ((lOps->write == NULL) || !lCfg.isWritable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lOps->isRangeValid != NULL) && !lOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lOps->write(lAddress, buffer, length);
}

static bool updateManagerEraseRegion(uint8_t regionId, uint32_t offset, uint32_t length)
{
    stUpdateRegionCfg lCfg;
    const stUpdateStorageOps *lOps;
    uint32_t lAddress;

    if ((length == 0U) || !updateManagerGetRegionOps(regionId, &lCfg, &lOps)) {
        return false;
    }

    if ((lOps->erase == NULL) || !lCfg.isWritable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lOps->isRangeValid != NULL) && !lOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lOps->erase(lAddress, length);
}

static bool updateManagerReadMetaPayload(uint8_t regionId, void *payload, uint32_t payloadSize)
{
    stUpdateRegionCfg lCfg;
    stUpdateMetaRecord lRecord;
    bool lHasValidRecord = false;
    uint32_t lSlotCount;
    uint32_t lSlotIndex;
    uint32_t lBestSequence = 0U;

    if ((payload == NULL) || (payloadSize > sizeof(lRecord.payload)) || !updatePortGetRegionMap(regionId, &lCfg) ||
        (lCfg.eraseUnit == 0U) || (lCfg.size < lCfg.eraseUnit)) {
        return false;
    }

    lSlotCount = lCfg.size / lCfg.eraseUnit;
    if (lSlotCount > 2U) {
        lSlotCount = 2U;
    }

    for (lSlotIndex = 0U; lSlotIndex < lSlotCount; lSlotIndex++) {
        uint32_t lPayloadCrc;
        uint32_t lHeaderCrc;

        if (!updateManagerReadRegion(regionId,
                                     lSlotIndex * lCfg.eraseUnit,
                                     (uint8_t *)&lRecord,
                                     sizeof(lRecord))) {
            continue;
        }

        lHeaderCrc = updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                                          (const uint8_t *)&lRecord,
                                                                          offsetof(stUpdateMetaRecord, headerCrc32)));
        if ((lRecord.recordMagic != UPDATE_META_RECORD_MAGIC) ||
            (lRecord.commitMarker != UPDATE_META_COMMIT_MARKER) ||
            (lRecord.payloadLength != payloadSize) ||
            (lRecord.headerCrc32 != lHeaderCrc)) {
            continue;
        }

        lPayloadCrc = updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                                           lRecord.payload,
                                                                           lRecord.payloadLength));
        if (lRecord.payloadCrc32 != lPayloadCrc) {
            continue;
        }

        if (!lHasValidRecord || (lRecord.sequence >= lBestSequence)) {
            lHasValidRecord = true;
            lBestSequence = lRecord.sequence;
            memcpy(payload, lRecord.payload, payloadSize);
        }
    }

    return lHasValidRecord;
}

static bool updateManagerIsHeaderValid(const stUpdateImageHeader *header, uint32_t maxImageSize, bool requireReady)
{
    if (header == NULL) {
        return false;
    }

    if ((header->magic != UPDATE_IMAGE_MAGIC) ||
        (header->headerVersion != UPDATE_HEADER_VERSION) ||
        (header->imageType != (uint32_t)E_UPDATE_IMAGE_TYPE_APP) ||
        (header->imageSize == 0U) ||
        (header->imageSize > maxImageSize) ||
        (header->headerCrc32 != updateManagerCalcHeaderCrc(header))) {
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

static bool updateManagerLoadBackupHeader(stUpdateImageHeader *header)
{
    stUpdateRegionCfg lBackupCfg;

    if ((header == NULL) || !updatePortGetRegionMap(E_UPDATE_REGION_BACKUP_APP, &lBackupCfg)) {
        return false;
    }

    if (!updateManagerReadMetaPayload(E_UPDATE_REGION_BACKUP_APP_HEADER, header, sizeof(*header))) {
        return false;
    }

    return updateManagerIsHeaderValid(header, lBackupCfg.size, true);
}

static bool updateManagerGetExecutableRegion(uint8_t regionId, stUpdateRegionCfg *cfg)
{
    if ((cfg == NULL) || !updatePortGetRegionMap(regionId, cfg)) {
        return false;
    }

    return cfg->isExecutable && (cfg->storageId == E_UPDATE_STORAGE_INTERNAL_FLASH) && (cfg->size >= 8U);
}

static bool updateManagerReadAppVector(const stUpdateRegionCfg *cfg, uint32_t vector[2])
{
    if ((cfg == NULL) || (vector == NULL)) {
        return false;
    }

    return drvMcuFlashRead(cfg->startAddress, (uint8_t *)vector, sizeof(uint32_t) * 2U);
}

static bool updateManagerValidateExecutableRegion(uint8_t regionId)
{
    stUpdateRegionCfg lRegionCfg;
    uint32_t lVector[2] = {0U, 0U};
    uint32_t lResetHandler;

    if (!updateManagerGetExecutableRegion(regionId, &lRegionCfg)) {
        return false;
    }

    if (!updateManagerReadAppVector(&lRegionCfg, lVector)) {
        return false;
    }

    if ((lVector[0] & UPDATE_APP_VECTOR_STACK_MASK) != UPDATE_APP_VECTOR_STACK_BASE) {
        return false;
    }

    if ((lVector[1] & 0x1U) == 0U) {
        return false;
    }

    lResetHandler = lVector[1] & UPDATE_APP_VECTOR_ENTRY_MASK;
    return (lResetHandler >= lRegionCfg.startAddress) &&
           (lResetHandler < (lRegionCfg.startAddress + lRegionCfg.size));
}

static bool updateManagerVerifyRegionCrc(uint8_t regionId, uint32_t imageSize, uint32_t expectedCrc32)
{
    uint32_t lOffset = 0U;
    uint32_t lCrc = 0xFFFFFFFFUL;

    if ((imageSize == 0U) || (expectedCrc32 == 0U)) {
        return false;
    }

    while (lOffset < imageSize) {
        uint32_t lChunkSize = imageSize - lOffset;

        if (lChunkSize > sizeof(gUpdateManagerCopyBuffer)) {
            lChunkSize = sizeof(gUpdateManagerCopyBuffer);
        }

        if (!updateManagerReadRegion(regionId, lOffset, gUpdateManagerCopyBuffer, lChunkSize)) {
            return false;
        }

        lCrc = updateManagerCrc32Update(lCrc, gUpdateManagerCopyBuffer, lChunkSize);
        lOffset += lChunkSize;
        updatePortFeedWatchdog();
    }

    return updateManagerCrc32Finalize(lCrc) == expectedCrc32;
}

static bool updateManagerRestoreAppFromBackup(void)
{
    stUpdateImageHeader lBackupHeader;
    stUpdateRegionCfg lRunAppCfg;
    uint32_t lOffset = 0U;

    if (!updateManagerLoadBackupHeader(&lBackupHeader)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "backup header invalid or missing");
        return false;
    }

    if (!updatePortGetRegionMap(E_UPDATE_REGION_RUN_APP, &lRunAppCfg) ||
        (lBackupHeader.imageSize > lRunAppCfg.size)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "backup image size out of range: %lu", (unsigned long)lBackupHeader.imageSize);
        return false;
    }

    LOG_W(UPDATE_MANAGER_LOG_TAG,
          "idle timeout rollback from backup, size=%lu crc=0x%08lX",
          (unsigned long)lBackupHeader.imageSize,
          (unsigned long)lBackupHeader.imageCrc32);

    while (lOffset < lRunAppCfg.size) {
        uint32_t lEraseSize = lRunAppCfg.eraseUnit;

        if (lEraseSize > (lRunAppCfg.size - lOffset)) {
            lEraseSize = lRunAppCfg.size - lOffset;
        }

        if (!updateManagerEraseRegion(E_UPDATE_REGION_RUN_APP, lOffset, lEraseSize)) {
            LOG_E(UPDATE_MANAGER_LOG_TAG, "erase run app failed at offset=%lu", (unsigned long)lOffset);
            return false;
        }

        lOffset += lEraseSize;
        updatePortFeedWatchdog();
    }

    lOffset = 0U;
    while (lOffset < lBackupHeader.imageSize) {
        uint32_t lChunkSize = lBackupHeader.imageSize - lOffset;

        if (lChunkSize > sizeof(gUpdateManagerCopyBuffer)) {
            lChunkSize = sizeof(gUpdateManagerCopyBuffer);
        }

        if (!updateManagerReadRegion(E_UPDATE_REGION_BACKUP_APP, lOffset, gUpdateManagerCopyBuffer, lChunkSize) ||
            !updateManagerWriteRegion(E_UPDATE_REGION_RUN_APP, lOffset, gUpdateManagerCopyBuffer, lChunkSize)) {
            LOG_E(UPDATE_MANAGER_LOG_TAG, "restore backup failed at offset=%lu", (unsigned long)lOffset);
            return false;
        }

        lOffset += lChunkSize;
        updatePortFeedWatchdog();
    }

    if (!updateManagerVerifyRegionCrc(E_UPDATE_REGION_RUN_APP,
                                      lBackupHeader.imageSize,
                                      lBackupHeader.imageCrc32)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "restored app crc mismatch");
        return false;
    }

    if (!updateManagerValidateExecutableRegion(E_UPDATE_REGION_RUN_APP)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "restored app vector invalid");
        return false;
    }

    if (!updateManagerSetBootRecordRunApp()) {
        LOG_W(UPDATE_MANAGER_LOG_TAG, "restore succeeded but boot record update failed");
    }

    return true;
}

static void updateManagerHandleIdleTimeout(void)
{
    LOG_W(UPDATE_MANAGER_LOG_TAG, "idle timeout reached, evaluating app and backup");

    if (updateManagerValidateExecutableRegion(E_UPDATE_REGION_RUN_APP)) {
        LOG_I(UPDATE_MANAGER_LOG_TAG, "run app valid after idle timeout, jump directly");
        (void)updateManagerJumpToRegionInternal(E_UPDATE_REGION_RUN_APP);
        return;
    }

    LOG_W(UPDATE_MANAGER_LOG_TAG, "run app invalid after idle timeout, restoring backup");
    if (updateManagerRestoreAppFromBackup()) {
        LOG_I(UPDATE_MANAGER_LOG_TAG, "backup restore complete, jump to app");
        (void)updateManagerJumpToRegionInternal(E_UPDATE_REGION_RUN_APP);
        return;
    }

    LOG_E(UPDATE_MANAGER_LOG_TAG, "idle timeout recovery failed");
}

static void updateManagerProcessIdleRecovery(const stUpdateStatus *status, uint32_t nowTickMs)
{
    if (status == NULL) {
        return;
    }

    if (status->isUpdateRequested) {
        gUpdateManagerState.isIdleTimerActive = false;
        gUpdateManagerState.isIdleRecoveryHandled = false;
        return;
    }

    if (gUpdateManagerState.isIdleRecoveryHandled) {
        return;
    }

    if (!gUpdateManagerState.isIdleTimerActive) {
        gUpdateManagerState.idleStartTickMs = nowTickMs;
        gUpdateManagerState.isIdleTimerActive = true;
        return;
    }

    if ((nowTickMs - gUpdateManagerState.idleStartTickMs) < UPDATE_MANAGER_IDLE_TIMEOUT_MS) {
        return;
    }

    gUpdateManagerState.isIdleTimerActive = false;
    gUpdateManagerState.isIdleRecoveryHandled = true;
    updateManagerHandleIdleTimeout();
}

static bool updateManagerJumpToRegionInternal(uint8_t regionId)
{
    stUpdateRegionCfg lRegionCfg;
    uint32_t lVector[2] = {0U, 0U};
    uint32_t lResetHandler;

    if (!updateManagerGetExecutableRegion(regionId, &lRegionCfg)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "jump region invalid, region=%u", (unsigned int)regionId);
        return false;
    }

    if (!updateManagerReadAppVector(&lRegionCfg, lVector)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "read app vector failed, base=0x%08lX", (unsigned long)lRegionCfg.startAddress);
        return false;
    }

    if ((lVector[0] & UPDATE_APP_VECTOR_STACK_MASK) != UPDATE_APP_VECTOR_STACK_BASE) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "invalid app stack pointer: 0x%08lX", (unsigned long)lVector[0]);
        return false;
    }

    if ((lVector[1] & 0x1U) == 0U) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "invalid app reset vector: 0x%08lX", (unsigned long)lVector[1]);
        return false;
    }

    lResetHandler = lVector[1] & UPDATE_APP_VECTOR_ENTRY_MASK;
    if ((lResetHandler < lRegionCfg.startAddress) ||
        (lResetHandler >= (lRegionCfg.startAddress + lRegionCfg.size))) {
        LOG_E(UPDATE_MANAGER_LOG_TAG,
              "app reset vector out of range: 0x%08lX",
              (unsigned long)lResetHandler);
        return false;
    }

    LOG_I(UPDATE_MANAGER_LOG_TAG,
          "jump to region %u: sp=0x%08lX reset=0x%08lX",
          (unsigned int)regionId,
          (unsigned long)lVector[0],
          (unsigned long)lVector[1]);
    updateManagerPrepareAppJump(lRegionCfg.startAddress);
    updateManagerLaunchApp(lVector[0], lVector[1]);
    return false;
}

static void updateManagerResetPeripheralState(void)
{
    __HAL_RCC_APB1_FORCE_RESET();
    __HAL_RCC_APB2_FORCE_RESET();

    __HAL_RCC_APB1_RELEASE_RESET();
    __HAL_RCC_APB2_RELEASE_RESET();

    EXTI->IMR = 0U;
    EXTI->EMR = 0U;
    EXTI->RTSR = 0U;
    EXTI->FTSR = 0U;
    EXTI->SWIER = 0U;
    EXTI->PR = 0xFFFFFFFFUL;

    DMA1->IFCR = 0x0FFFFFFFUL;
#if defined(DMA2)
    DMA2->IFCR = 0x0FFFFFFFUL;
#endif
}

static void updateManagerPrepareAppJump(uint32_t vectorBase)
{
    uint32_t lIndex;

    __disable_irq();
    HAL_DeInit();
    HAL_RCC_DeInit();
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    updateManagerResetPeripheralState();

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

static __NO_RETURN __ASM void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler)
{
    MSR MSP, r0
    MOVS r0, #0
    MSR PRIMASK, r0
    DSB
    ISB
    BX  r1
}

void updatePortFeedWatchdog(void)
{
    Drv_WatchDogFeed();
}

bool updatePortJumpToRegion(uint8_t regionId)
{
    return updateManagerJumpToRegionInternal(regionId);
}

/**************************End of file********************************/
