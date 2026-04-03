/************************************************************************************
* @file     : bspspi.h
* @brief    : STM32 HAL SPI BSP adapter for drvspi.
***********************************************************************************/
#ifndef BSPSPI_H
#define BSPSPI_H

#include <stdbool.h>
#include <stdint.h>

#include "drvspi.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stBspSpiCsPin {
    GPIO_TypeDef *gpioPort;
    uint16_t gpioPin;
    bool isActiveLow;
} stBspSpiCsPin;

eDrvStatus bspSpiInit(eDrvSpiPortMap spi);
eDrvStatus bspSpiTransfer(eDrvSpiPortMap spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs);
void bspSpiCsInit(void *context);
void bspSpiCsWrite(void *context, bool isActive);

#ifdef __cplusplus
}
#endif

#endif  // BSPSPI_H
/**************************End of file********************************/
