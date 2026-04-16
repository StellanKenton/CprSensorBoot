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
#define UPDATE_MANAGER_CRC32_INIT_VALUE    0xFFFFFFFFUL

static bool updateManagerCanBootNormally(const stUpdateBootRecord *record);
static uint32_t updateManagerCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length);
static uint32_t updateManagerCrc32Finalize(uint32_t crc);
static uint32_t updateManagerCalcBootRecordCrc(const stUpdateBootRecord *record);
static uint32_t updateManagerCalcMetaHeaderCrc(const stUpdateMetaRecord *record);
static bool updateManagerIsMetaRecordValid(const stUpdateMetaRecord *record, uint32_t payloadSize);
static uint32_t updateManagerGetNextSequence(uint32_t currentSequence);
static bool updateManagerReadBootMetaSequence(uint32_t *sequenceOut);
static bool updateManagerStoreBootRecord(const stUpdateBootRecord *record);
static bool updateManagerGetExecutableRegion(uint8_t regionId, stUpdateRegionCfg *cfg);
static bool updateManagerReadAppVector(const stUpdateRegionCfg *cfg, uint32_t vector[2]);
static bool updateManagerJumpToRegionInternal(uint8_t regionId);
static void updateManagerPrepareAppJump(uint32_t vectorBase);
static void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler);

void updateManagerReset(void)
{
    updateReset();
}

bool updateManagerInit(void)
{
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
        (void)updateJumpToTargetIfValid();
    }
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

    if (!updateManagerStoreBootRecord(&lRecord)) {
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

static uint32_t updateManagerCalcBootRecordCrc(const stUpdateBootRecord *record)
{
    if (record == NULL) {
        return 0U;
    }

    return updateManagerCrc32Finalize(updateManagerCrc32Update(UPDATE_MANAGER_CRC32_INIT_VALUE,
                                                               (const uint8_t *)record,
                                                               offsetof(stUpdateBootRecord, recordCrc32)));
}

static uint32_t updateManagerCalcMetaHeaderCrc(const stUpdateMetaRecord *record)
{
    if (record == NULL) {
        return 0U;
    }

    return updateManagerCrc32Finalize(updateManagerCrc32Update(UPDATE_MANAGER_CRC32_INIT_VALUE,
                                                               (const uint8_t *)record,
                                                               offsetof(stUpdateMetaRecord, headerCrc32)));
}

static bool updateManagerIsMetaRecordValid(const stUpdateMetaRecord *record, uint32_t payloadSize)
{
    uint32_t lPayloadCrc;

    if (record == NULL) {
        return false;
    }

    if ((record->recordMagic != UPDATE_META_RECORD_MAGIC) ||
        (record->commitMarker != UPDATE_META_COMMIT_MARKER) ||
        (record->payloadLength != payloadSize) ||
        (record->headerCrc32 != updateManagerCalcMetaHeaderCrc(record))) {
        return false;
    }

    lPayloadCrc = updateManagerCrc32Finalize(updateManagerCrc32Update(UPDATE_MANAGER_CRC32_INIT_VALUE,
                                                                      record->payload,
                                                                      record->payloadLength));
    return record->payloadCrc32 == lPayloadCrc;
}

static uint32_t updateManagerGetNextSequence(uint32_t currentSequence)
{
    if ((currentSequence == 0U) || (currentSequence == 0xFFFFFFFFUL)) {
        return 1U;
    }

    return currentSequence + 1U;
}

static bool updateManagerReadBootMetaSequence(uint32_t *sequenceOut)
{
    stUpdateRegionCfg lRegionCfg;
    stUpdateMetaRecord lMetaRecord;
    uint32_t lSlotCount;
    uint32_t lSlotIndex;
    uint32_t lBestSequence = 0U;
    bool lHasValidRecord = false;

    if ((sequenceOut == NULL) || !updatePortGetRegionMap(E_UPDATE_REGION_BOOT_RECORD, &lRegionCfg)) {
        return false;
    }

    if ((lRegionCfg.storageId != E_UPDATE_STORAGE_INTERNAL_FLASH) || (lRegionCfg.eraseUnit == 0U) ||
        (lRegionCfg.size < lRegionCfg.eraseUnit)) {
        return false;
    }

    lSlotCount = lRegionCfg.size / lRegionCfg.eraseUnit;
    if (lSlotCount > 2U) {
        lSlotCount = 2U;
    }

    for (lSlotIndex = 0U; lSlotIndex < lSlotCount; lSlotIndex++) {
        uint32_t lAddress = lRegionCfg.startAddress + (lSlotIndex * lRegionCfg.eraseUnit);

        if (!drvMcuFlashRead(lAddress, (uint8_t *)&lMetaRecord, sizeof(lMetaRecord))) {
            continue;
        }

        if (!updateManagerIsMetaRecordValid(&lMetaRecord, sizeof(stUpdateBootRecord))) {
            continue;
        }

        if (!lHasValidRecord || (lMetaRecord.sequence >= lBestSequence)) {
            lHasValidRecord = true;
            lBestSequence = lMetaRecord.sequence;
        }
    }

    *sequenceOut = lBestSequence;
    return true;
}

static bool updateManagerStoreBootRecord(const stUpdateBootRecord *record)
{
    stUpdateRegionCfg lRegionCfg;
    stUpdateMetaRecord lMetaRecord;
    stUpdateMetaRecord lReadback;
    stUpdateBootRecord lBootRecord;
    uint32_t lCommitMarker = UPDATE_META_COMMIT_MARKER;
    uint32_t lCurrentSequence;
    uint32_t lNextSequence;
    uint32_t lTargetSlot;
    uint32_t lBaseAddress;
    uint32_t lCommitAddress;

    if ((record == NULL) || !updatePortGetRegionMap(E_UPDATE_REGION_BOOT_RECORD, &lRegionCfg)) {
        return false;
    }

    if ((lRegionCfg.storageId != E_UPDATE_STORAGE_INTERNAL_FLASH) || (lRegionCfg.eraseUnit == 0U) ||
        (lRegionCfg.size < (2U * lRegionCfg.eraseUnit))) {
        return false;
    }

    if (!updateManagerReadBootMetaSequence(&lCurrentSequence)) {
        return false;
    }

    lNextSequence = updateManagerGetNextSequence(lCurrentSequence);
    lTargetSlot = ((lNextSequence > 1U) ? ((lNextSequence - 1U) % 2U) : 0U);
    lBaseAddress = lRegionCfg.startAddress + (lTargetSlot * lRegionCfg.eraseUnit);

    lBootRecord = *record;
    lBootRecord.sequence = lNextSequence;
    lBootRecord.recordCrc32 = updateManagerCalcBootRecordCrc(&lBootRecord);

    (void)memset(&lMetaRecord, 0xFF, sizeof(lMetaRecord));
    lMetaRecord.recordMagic = UPDATE_META_RECORD_MAGIC;
    lMetaRecord.sequence = lNextSequence;
    lMetaRecord.payloadLength = sizeof(stUpdateBootRecord);
    memcpy(lMetaRecord.payload, &lBootRecord, sizeof(lBootRecord));
    lMetaRecord.payloadCrc32 = updateManagerCrc32Finalize(updateManagerCrc32Update(UPDATE_MANAGER_CRC32_INIT_VALUE,
                                                                                   lMetaRecord.payload,
                                                                                   lMetaRecord.payloadLength));
    lMetaRecord.headerCrc32 = updateManagerCalcMetaHeaderCrc(&lMetaRecord);

    if (!drvMcuFlashErase(lBaseAddress, lRegionCfg.eraseUnit)) {
        return false;
    }

    if (!drvMcuFlashWrite(lBaseAddress,
                          (const uint8_t *)&lMetaRecord,
                          offsetof(stUpdateMetaRecord, commitMarker))) {
        return false;
    }

    if (!drvMcuFlashRead(lBaseAddress,
                         (uint8_t *)&lReadback,
                         offsetof(stUpdateMetaRecord, commitMarker))) {
        return false;
    }

    if ((lReadback.recordMagic != lMetaRecord.recordMagic) ||
        (lReadback.sequence != lMetaRecord.sequence) ||
        (lReadback.payloadLength != lMetaRecord.payloadLength) ||
        (lReadback.payloadCrc32 != lMetaRecord.payloadCrc32) ||
        (lReadback.headerCrc32 != lMetaRecord.headerCrc32)) {
        return false;
    }

    lCommitAddress = lBaseAddress + offsetof(stUpdateMetaRecord, commitMarker);
    return drvMcuFlashWrite(lCommitAddress,
                            (const uint8_t *)&lCommitMarker,
                            sizeof(lCommitMarker));
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

static void updateManagerPrepareAppJump(uint32_t vectorBase)
{
    uint32_t lIndex;

    __disable_irq();
    HAL_DeInit();
    HAL_RCC_DeInit();

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

static void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler)
{
    __set_MSP(stackPointer);
    __set_PRIMASK(0U);
    __DSB();
    __ISB();

    ((void (*)(void))resetHandler)();

    while (1) {
    }
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
