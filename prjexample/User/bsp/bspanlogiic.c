/***********************************************************************************
* @file     : bspanlogiic.c
* @brief    : Board-level software IIC BSP implementation.
* @details  : Maps logical software IIC buses to the PCA9535 and TM1651 GPIOs.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspanlogiic.h"

#include "main.h"

#include "../port/drvanlogiic_port.h"

static bool gBspAnlogIicCycleCntReady = false;

static void bspAnlogIicEnableCycleCnt(void);

void bspAnlogIicInit(uint8_t iic)
{
    bspAnlogIicSetSda(iic, true);
    bspAnlogIicSetScl(iic, true);
}

void bspAnlogIicSetScl(uint8_t iic, bool releaseHigh)
{
    switch ((eDrvAnlogIicPortMap)iic) {
        case DRVANLOGIIC_PCA:
            HAL_GPIO_WritePin(PCA9535_SCL_GPIO_Port,
                              PCA9535_SCL_Pin,
                              releaseHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case DRVANLOGIIC_TM:
            HAL_GPIO_WritePin(MCU_LED_CLK_GPIO_Port,
                              MCU_LED_CLK_Pin,
                              releaseHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        default:
            break;
    }
}

void bspAnlogIicSetSda(uint8_t iic, bool releaseHigh)
{
    switch ((eDrvAnlogIicPortMap)iic) {
        case DRVANLOGIIC_PCA:
            HAL_GPIO_WritePin(PCA9535_SDA_GPIO_Port,
                              PCA9535_SDA_Pin,
                              releaseHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case DRVANLOGIIC_TM:
            HAL_GPIO_WritePin(MCU_LED_SDA_GPIO_Port,
                              MCU_LED_SDA_Pin,
                              releaseHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        default:
            break;
    }
}

bool bspAnlogIicReadScl(uint8_t iic)
{
    switch ((eDrvAnlogIicPortMap)iic) {
        case DRVANLOGIIC_PCA:
            return HAL_GPIO_ReadPin(PCA9535_SCL_GPIO_Port, PCA9535_SCL_Pin) == GPIO_PIN_SET;
        case DRVANLOGIIC_TM:
            return HAL_GPIO_ReadPin(MCU_LED_CLK_GPIO_Port, MCU_LED_CLK_Pin) == GPIO_PIN_SET;
        default:
            return false;
    }
}

bool bspAnlogIicReadSda(uint8_t iic)
{
    switch ((eDrvAnlogIicPortMap)iic) {
        case DRVANLOGIIC_PCA:
            return HAL_GPIO_ReadPin(PCA9535_SDA_GPIO_Port, PCA9535_SDA_Pin) == GPIO_PIN_SET;
        case DRVANLOGIIC_TM:
            return HAL_GPIO_ReadPin(MCU_LED_SDA_GPIO_Port, MCU_LED_SDA_Pin) == GPIO_PIN_SET;
        default:
            return false;
    }
}

void bspAnlogIicDelayUs(uint16_t delayUs)
{
    uint32_t lCyclesPerUs;
    uint32_t lWaitCycles;
    uint32_t lStartCycles;

    if (delayUs == 0U) {
        return;
    }

    bspAnlogIicEnableCycleCnt();
    lCyclesPerUs = SystemCoreClock / 1000000U;
    if (lCyclesPerUs == 0U) {
        lCyclesPerUs = 1U;
    }

    lWaitCycles = lCyclesPerUs * delayUs;
    lStartCycles = DWT->CYCCNT;
    while ((DWT->CYCCNT - lStartCycles) < lWaitCycles) {
    }
}

static void bspAnlogIicEnableCycleCnt(void)
{
    if (gBspAnlogIicCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gBspAnlogIicCycleCntReady = true;
}
/**************************End of file********************************/
