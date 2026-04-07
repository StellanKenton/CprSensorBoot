/************************************************************************************
* @file     : bsp_uart.c
* @brief    : STM32 HAL UART BSP adapter for drvuart.
***********************************************************************************/
#include "bsp_uart.h"

#include <stdbool.h>

#include "usart.h"
#include "ringbuffer.h"

static bool gBspUartReady[DRVUART_MAX];
static uint8_t gBspUartRxDmaBuf[DRVUART_MAX][BSPUART_RX_DMA_BUFFER_SIZE];
static uint8_t gBspUartRxRingStorage[DRVUART_MAX][BSPUART_RX_RING_BUFFER_SIZE];
static stRingBuffer gBspUartRxRing[DRVUART_MAX];

static UART_HandleTypeDef *bspUartGetHandle(eDrvUartPortMap uart);
static eDrvUartPortMap bspUartGetPortByHandle(UART_HandleTypeDef *handle, bool *isMatched);
static eDrvStatus bspUartMapHalStatus(HAL_StatusTypeDef status);
static eDrvStatus bspUartEnsureReady(eDrvUartPortMap uart);
static eDrvStatus bspUartStartRx(eDrvUartPortMap uart);
static void bspUartCacheRxData(eDrvUartPortMap uart, uint16_t length);

eDrvStatus bspUartInit(eDrvUartPortMap uart)
{
    UART_HandleTypeDef *lHandle;

    lHandle = bspUartGetHandle(uart);
    if (lHandle == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (gBspUartReady[uart]) {
        return DRV_STATUS_OK;
    }

    MX_UART4_Init();

    if (ringBufferInit(&gBspUartRxRing[uart], gBspUartRxRingStorage[uart], BSPUART_RX_RING_BUFFER_SIZE) != RINGBUFFER_OK) {
        return DRV_STATUS_ERROR;
    }

    if (bspUartStartRx(uart) != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    gBspUartReady[uart] = true;
    return DRV_STATUS_OK;
}

eDrvStatus bspUartTransmit(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    UART_HandleTypeDef *lHandle;
    uint32_t lTimeout = (timeoutMs == 0U) ? 1U : timeoutMs;

    if ((buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (bspUartEnsureReady(uart) != DRV_STATUS_OK) {
        return DRV_STATUS_NOT_READY;
    }

    lHandle = bspUartGetHandle(uart);
    if (lHandle == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartMapHalStatus(HAL_UART_Transmit(lHandle, (uint8_t *)buffer, length, lTimeout));
}

eDrvStatus bspUartTransmitIt(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *lHandle;

    if ((buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (bspUartEnsureReady(uart) != DRV_STATUS_OK) {
        return DRV_STATUS_NOT_READY;
    }

    lHandle = bspUartGetHandle(uart);
    if (lHandle == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartMapHalStatus(HAL_UART_Transmit_IT(lHandle, (uint8_t *)buffer, length));
}

eDrvStatus bspUartTransmitDma(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *lHandle;

    if ((buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (bspUartEnsureReady(uart) != DRV_STATUS_OK) {
        return DRV_STATUS_NOT_READY;
    }

    lHandle = bspUartGetHandle(uart);
    if (lHandle == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartMapHalStatus(HAL_UART_Transmit_DMA(lHandle, (uint8_t *)buffer, length));
}

uint16_t bspUartGetDataLen(eDrvUartPortMap uart)
{
    uint32_t lUsed;

    if (bspUartEnsureReady(uart) != DRV_STATUS_OK) {
        return 0U;
    }

    lUsed = ringBufferGetUsed(&gBspUartRxRing[uart]);
    if (lUsed > UINT16_MAX) {
        return UINT16_MAX;
    }

    return (uint16_t)lUsed;
}

eDrvStatus bspUartReceive(eDrvUartPortMap uart, uint8_t *buffer, uint16_t length)
{
    if ((buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (bspUartEnsureReady(uart) != DRV_STATUS_OK) {
        return DRV_STATUS_NOT_READY;
    }

    if (ringBufferGetUsed(&gBspUartRxRing[uart]) < (uint32_t)length) {
        return DRV_STATUS_NOT_READY;
    }

    if (ringBufferRead(&gBspUartRxRing[uart], buffer, length) != (uint32_t)length) {
        return DRV_STATUS_ERROR;
    }

    return DRV_STATUS_OK;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    bool lIsMatched = false;
    eDrvUartPortMap lUart = bspUartGetPortByHandle(huart, &lIsMatched);

    if (!lIsMatched) {
        return;
    }

    bspUartCacheRxData(lUart, size);
    (void)bspUartStartRx(lUart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    bool lIsMatched = false;
    eDrvUartPortMap lUart = bspUartGetPortByHandle(huart, &lIsMatched);

    if (!lIsMatched) {
        return;
    }

    (void)HAL_UART_AbortReceive(huart);
    (void)bspUartStartRx(lUart);
}

static UART_HandleTypeDef *bspUartGetHandle(eDrvUartPortMap uart)
{
    switch (uart) {
        case DRVUART_DEBUG:
            return &huart4;
        default:
            return NULL;
    }
}

static eDrvUartPortMap bspUartGetPortByHandle(UART_HandleTypeDef *handle, bool *isMatched)
{
    if (isMatched != NULL) {
        *isMatched = false;
    }

    if (handle == &huart4) {
        if (isMatched != NULL) {
            *isMatched = true;
        }
        return DRVUART_DEBUG;
    }

    return DRVUART_MAX;
}

static eDrvStatus bspUartMapHalStatus(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK:
            return DRV_STATUS_OK;
        case HAL_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        case HAL_BUSY:
            return DRV_STATUS_BUSY;
        case HAL_ERROR:
        default:
            return DRV_STATUS_ERROR;
    }
}

static eDrvStatus bspUartEnsureReady(eDrvUartPortMap uart)
{
    if (uart >= DRVUART_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gBspUartReady[uart]) {
        return bspUartInit(uart);
    }

    return DRV_STATUS_OK;
}

static eDrvStatus bspUartStartRx(eDrvUartPortMap uart)
{
    UART_HandleTypeDef *lHandle;
    HAL_StatusTypeDef lHalStatus;

    lHandle = bspUartGetHandle(uart);
    if (lHandle == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lHalStatus = HAL_UARTEx_ReceiveToIdle_DMA(lHandle, gBspUartRxDmaBuf[uart], BSPUART_RX_DMA_BUFFER_SIZE);
    if (lHalStatus != HAL_OK) {
        return bspUartMapHalStatus(lHalStatus);
    }

    if (lHandle->hdmarx != NULL) {
        __HAL_DMA_DISABLE_IT(lHandle->hdmarx, DMA_IT_HT);
    }

    return DRV_STATUS_OK;
}

static void bspUartCacheRxData(eDrvUartPortMap uart, uint16_t length)
{
    if ((uart >= DRVUART_MAX) || (length == 0U)) {
        return;
    }

    if ((uint32_t)length > BSPUART_RX_DMA_BUFFER_SIZE) {
        length = BSPUART_RX_DMA_BUFFER_SIZE;
    }

    (void)ringBufferWriteOverwrite(&gBspUartRxRing[uart], gBspUartRxDmaBuf[uart], length);
}
/**************************End of file********************************/
