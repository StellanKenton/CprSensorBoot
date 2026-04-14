/***********************************************************************************
* @file     : bspgpio.c
* @brief    : Board-level GPIO BSP implementation.
* @details  : Maps project logical GPIO pins to the concrete STM32 GPIO resources.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspgpio.h"

#include <stdbool.h>

#include "main.h"

#include "../port/drvgpio_port.h"

typedef struct stBspGpioMap {
    GPIO_TypeDef *gpioPort;
    uint16_t gpioPin;
    bool isOutput;
} stBspGpioMap;

static const stBspGpioMap gBspGpioMap[DRVGPIO_MAX] = {
    [DRVGPIO_EN_AUDIO] = {
        .gpioPort = EN_AUDIO_GPIO_Port,
        .gpioPin = EN_AUDIO_Pin,
        .isOutput = true,
    },
    [DRVGPIO_RESET_WIFI] = {
        .gpioPort = RESET_WIFI_GPIO_Port,
        .gpioPin = RESET_WIFI_Pin,
        .isOutput = true,
    },
    [DRVGPIO_USB_SELECT] = {
        .gpioPort = USB_Select_GPIO_Port,
        .gpioPin = USB_Select_Pin,
        .isOutput = true,
    },
    [DRVGPIO_POWER_ON_CHECK] = {
        .gpioPort = Power_ON_Check_GPIO_Port,
        .gpioPin = Power_ON_Check_Pin,
        .isOutput = false,
    },
};

void bspGpioInit(void)
{
    HAL_GPIO_WritePin(EN_AUDIO_GPIO_Port, EN_AUDIO_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RESET_WIFI_GPIO_Port, RESET_WIFI_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(USB_Select_GPIO_Port, USB_Select_Pin, GPIO_PIN_RESET);
}

void bspGpioWrite(uint8_t pin, eDrvGpioPinState state)
{
    if ((pin >= DRVGPIO_MAX) || !gBspGpioMap[pin].isOutput) {
        return;
    }

    HAL_GPIO_WritePin(gBspGpioMap[pin].gpioPort,
                      gBspGpioMap[pin].gpioPin,
                      (state == DRVGPIO_PIN_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

eDrvGpioPinState bspGpioRead(uint8_t pin)
{
    if (pin >= DRVGPIO_MAX) {
        return DRVGPIO_PIN_STATE_INVALID;
    }

    return (HAL_GPIO_ReadPin(gBspGpioMap[pin].gpioPort, gBspGpioMap[pin].gpioPin) == GPIO_PIN_SET) ?
        DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET;
}

void bspGpioToggle(uint8_t pin)
{
    if ((pin >= DRVGPIO_MAX) || !gBspGpioMap[pin].isOutput) {
        return;
    }

    HAL_GPIO_TogglePin(gBspGpioMap[pin].gpioPort, gBspGpioMap[pin].gpioPin);
}
/**************************End of file********************************/
