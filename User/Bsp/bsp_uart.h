/************************************************************************************
* @file     : bsp_uart.h
* @brief    : STM32 HAL UART BSP adapter for drvuart.
***********************************************************************************/
#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

#include "main.h"
#include "rep_config.h"
#include "drvuart_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BSPUART_RX_DMA_BUFFER_SIZE
#define BSPUART_RX_DMA_BUFFER_SIZE      256U
#endif

#ifndef BSPUART_RX_RING_BUFFER_SIZE
#define BSPUART_RX_RING_BUFFER_SIZE     1024U
#endif

eDrvStatus bspUartInit(eDrvUartPortMap uart);
eDrvStatus bspUartTransmit(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus bspUartTransmitIt(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length);
eDrvStatus bspUartTransmitDma(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length);
uint16_t bspUartGetDataLen(eDrvUartPortMap uart);
eDrvStatus bspUartReceive(eDrvUartPortMap uart, uint8_t *buffer, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif  // BSP_UART_H
/**************************End of file********************************/
