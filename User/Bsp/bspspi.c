/************************************************************************************
* @file     : bspspi.c
* @brief    : STM32 HAL SPI BSP adapter for drvspi.
***********************************************************************************/
#include "bspspi.h"

#include "spi.h"

static bool gBspSpiBus0Ready = false;

static SPI_HandleTypeDef *bspSpiGetHandle(eDrvSpiPortMap spi);
static eDrvStatus bspSpiMapHalStatus(HAL_StatusTypeDef status);
static void bspSpiEnableGpioClock(GPIO_TypeDef *gpioPort);

eDrvStatus bspSpiInit(eDrvSpiPortMap spi)
{
    if (bspSpiGetHandle(spi) == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gBspSpiBus0Ready) {
        MX_SPI1_Init();
        gBspSpiBus0Ready = true;
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspSpiTransfer(eDrvSpiPortMap spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs)
{
    SPI_HandleTypeDef *lHandle;
    HAL_StatusTypeDef lHalStatus;
    uint32_t lTimeout = (timeoutMs == 0U) ? 1U : timeoutMs;
    uint16_t lIndex;
    uint8_t lTxByte;
    uint8_t lRxByte;

    if (length == 0U) {
        return DRV_STATUS_OK;
    }

    lHandle = bspSpiGetHandle(spi);
    if (lHandle == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gBspSpiBus0Ready) {
        eDrvStatus lInitStatus = bspSpiInit(spi);
        if (lInitStatus != DRV_STATUS_OK) {
            return lInitStatus;
        }
    }

    if ((txBuffer != NULL) && (rxBuffer != NULL)) {
        lHalStatus = HAL_SPI_TransmitReceive(lHandle, (uint8_t *)txBuffer, rxBuffer, length, lTimeout);
        return bspSpiMapHalStatus(lHalStatus);
    }

    if (txBuffer != NULL) {
        lHalStatus = HAL_SPI_Transmit(lHandle, (uint8_t *)txBuffer, length, lTimeout);
        return bspSpiMapHalStatus(lHalStatus);
    }

    for (lIndex = 0U; lIndex < length; ++lIndex) {
        lTxByte = fillData;
        lRxByte = 0U;
        lHalStatus = HAL_SPI_TransmitReceive(lHandle, &lTxByte, &lRxByte, 1U, lTimeout);
        if (lHalStatus != HAL_OK) {
            return bspSpiMapHalStatus(lHalStatus);
        }

        if (rxBuffer != NULL) {
            rxBuffer[lIndex] = lRxByte;
        }
    }

    return DRV_STATUS_OK;
}

void bspSpiCsInit(void *context)
{
    GPIO_InitTypeDef lGpioInit = {0};
    stBspSpiCsPin *lCsPin = (stBspSpiCsPin *)context;

    if ((lCsPin == NULL) || (lCsPin->gpioPort == NULL)) {
        return;
    }

    bspSpiEnableGpioClock(lCsPin->gpioPort);

    lGpioInit.Pin = lCsPin->gpioPin;
    lGpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    lGpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(lCsPin->gpioPort, &lGpioInit);

    bspSpiCsWrite(context, false);
}

void bspSpiCsWrite(void *context, bool isActive)
{
    GPIO_PinState lPinState;
    stBspSpiCsPin *lCsPin = (stBspSpiCsPin *)context;

    if ((lCsPin == NULL) || (lCsPin->gpioPort == NULL)) {
        return;
    }

    lPinState = ((lCsPin->isActiveLow ? !isActive : isActive) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(lCsPin->gpioPort, lCsPin->gpioPin, lPinState);
}

static SPI_HandleTypeDef *bspSpiGetHandle(eDrvSpiPortMap spi)
{
    switch (spi) {
        case DRVSPI_BUS0:
            return &hspi1;
        default:
            return NULL;
    }
}

static eDrvStatus bspSpiMapHalStatus(HAL_StatusTypeDef status)
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

static void bspSpiEnableGpioClock(GPIO_TypeDef *gpioPort)
{
    if (gpioPort == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        return;
    }

    if (gpioPort == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        return;
    }

    if (gpioPort == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        return;
    }

#ifdef GPIOD
    if (gpioPort == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        return;
    }
#endif

#ifdef GPIOE
    if (gpioPort == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        return;
    }
#endif
}
/**************************End of file********************************/
