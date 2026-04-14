/***********************************************************************************
* @file     : bspuart.c
* @brief    : Board-level UART BSP implementation.
* @details  : Maps logical UARTs to STM32 HAL UART instances and raw RX storage.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspuart.h"

#include <stddef.h>

#include "usart.h"

#include "../port/drvuart_port.h"

uint8_t gBspUartRxStorageDebug[DRVUART_RECVLEN_DEBUGUART];
uint8_t gBspUartRxStorageAudio[DRVUART_RECVLEN_AUDIO];

static eDrvStatus bspUartStatusFromHal(HAL_StatusTypeDef halStatus)
{
    switch (halStatus) {
        case HAL_OK:
            return DRV_STATUS_OK;
        case HAL_BUSY:
            return DRV_STATUS_BUSY;
        case HAL_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        default:
            return DRV_STATUS_ERROR;
    }
}

static UART_HandleTypeDef *bspUartGetHandle(uint8_t uart)
{
    switch ((eDrvUartPortMap)uart) {
        case DRVUART_DEBUG:
            return &huart4;
        case DRVUART_AUDIO:
            return &huart2;
        default:
            return NULL;
    }
}

eDrvStatus bspUartInit(uint8_t uart)
{
    UART_HandleTypeDef *handle = bspUartGetHandle(uart);

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    UART_HandleTypeDef *handle = bspUartGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartStatusFromHal(HAL_UART_Transmit(handle, (uint8_t *)buffer, length, timeoutMs));
}

eDrvStatus bspUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *handle = bspUartGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartStatusFromHal(HAL_UART_Transmit_IT(handle, (uint8_t *)buffer, length));
}

eDrvStatus bspUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *handle = bspUartGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartStatusFromHal(HAL_UART_Transmit_DMA(handle, (uint8_t *)buffer, length));
}

uint16_t bspUartGetDataLen(uint8_t uart)
{
    (void)uart;
    return 0U;
}

eDrvStatus bspUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *handle = bspUartGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartStatusFromHal(HAL_UART_Receive(handle, buffer, length, 0U));
}
/**************************End of file********************************/
