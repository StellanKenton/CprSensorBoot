#include "bspanlogiic.h"

typedef struct stBspAnlogIicPin {
    GPIO_TypeDef *gpioPort;
    uint16_t gpioPin;
} stBspAnlogIicPin;

typedef struct stBspAnlogIicBus {
    stBspAnlogIicPin scl;
    stBspAnlogIicPin sda;
} stBspAnlogIicBus;

static bool gBspAnlogIicCycleCntReady = false;

static const stBspAnlogIicBus gBspAnlogIicBusMap[DRVANLOGIIC_MAX] = {
    [DRVANLOGIIC_PCA] = {
        .scl = {PCA9535_SCL_GPIO_Port, PCA9535_SCL_Pin},
        .sda = {PCA9535_SDA_GPIO_Port, PCA9535_SDA_Pin},
    },
    [DRVANLOGIIC_TM] = {
        .scl = {MCU_LED_CLK_GPIO_Port, MCU_LED_CLK_Pin},
        .sda = {MCU_LED_SDA_GPIO_Port, MCU_LED_SDA_Pin},
    },
};

static const stBspAnlogIicBus *bspAnlogIicGetBus(eDrvAnlogIicPortMap iic);
static void bspAnlogIicEnableGpioClock(GPIO_TypeDef *gpioPort);
static void bspAnlogIicInitLine(const stBspAnlogIicPin *pin);
static void bspAnlogIicWriteLine(const stBspAnlogIicPin *pin, bool releaseHigh);
static bool bspAnlogIicReadLine(const stBspAnlogIicPin *pin);
static void bspAnlogIicEnableCycleCnt(void);

void bspAnlogIicInit(eDrvAnlogIicPortMap iic)
{
    const stBspAnlogIicBus *lBus;

    lBus = bspAnlogIicGetBus(iic);
    if (lBus == NULL) {
        return;
    }

    bspAnlogIicInitLine(&lBus->scl);
    bspAnlogIicInitLine(&lBus->sda);
    bspAnlogIicWriteLine(&lBus->scl, true);
    bspAnlogIicWriteLine(&lBus->sda, true);
}

void bspAnlogIicSetScl(eDrvAnlogIicPortMap iic, bool releaseHigh)
{
    const stBspAnlogIicBus *lBus;

    lBus = bspAnlogIicGetBus(iic);
    if (lBus == NULL) {
        return;
    }

    bspAnlogIicWriteLine(&lBus->scl, releaseHigh);
}

void bspAnlogIicSetSda(eDrvAnlogIicPortMap iic, bool releaseHigh)
{
    const stBspAnlogIicBus *lBus;

    lBus = bspAnlogIicGetBus(iic);
    if (lBus == NULL) {
        return;
    }

    bspAnlogIicWriteLine(&lBus->sda, releaseHigh);
}

bool bspAnlogIicReadScl(eDrvAnlogIicPortMap iic)
{
    const stBspAnlogIicBus *lBus;

    lBus = bspAnlogIicGetBus(iic);
    if (lBus == NULL) {
        return false;
    }

    return bspAnlogIicReadLine(&lBus->scl);
}

bool bspAnlogIicReadSda(eDrvAnlogIicPortMap iic)
{
    const stBspAnlogIicBus *lBus;

    lBus = bspAnlogIicGetBus(iic);
    if (lBus == NULL) {
        return false;
    }

    return bspAnlogIicReadLine(&lBus->sda);
}

void bspAnlogIicDelayUs(uint16_t delayUs)
{
    uint32_t lCyclesPerUs;
    uint32_t lStartCycles;
    uint32_t lWaitCycles;

    if (delayUs == 0U) {
        return;
    }

    bspAnlogIicEnableCycleCnt();
    lCyclesPerUs = SystemCoreClock / 1000000U;
    if (lCyclesPerUs == 0U) {
        lCyclesPerUs = 1U;
    }

    lWaitCycles = lCyclesPerUs * (uint32_t)delayUs;
    lStartCycles = DWT->CYCCNT;
    while ((DWT->CYCCNT - lStartCycles) < lWaitCycles) {
        __NOP();
    }
}

static const stBspAnlogIicBus *bspAnlogIicGetBus(eDrvAnlogIicPortMap iic)
{
    if ((uint32_t)iic >= (uint32_t)DRVANLOGIIC_MAX) {
        return NULL;
    }

    return &gBspAnlogIicBusMap[iic];
}

static void bspAnlogIicEnableGpioClock(GPIO_TypeDef *gpioPort)
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

static void bspAnlogIicInitLine(const stBspAnlogIicPin *pin)
{
    GPIO_InitTypeDef lGpioInit = {0};

    if ((pin == NULL) || (pin->gpioPort == NULL)) {
        return;
    }

    bspAnlogIicEnableGpioClock(pin->gpioPort);

    lGpioInit.Pin = pin->gpioPin;
    lGpioInit.Mode = GPIO_MODE_OUTPUT_OD;
    lGpioInit.Pull = GPIO_NOPULL;
    lGpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(pin->gpioPort, &lGpioInit);
}

static void bspAnlogIicWriteLine(const stBspAnlogIicPin *pin, bool releaseHigh)
{
    if ((pin == NULL) || (pin->gpioPort == NULL)) {
        return;
    }

    HAL_GPIO_WritePin(pin->gpioPort, pin->gpioPin, releaseHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool bspAnlogIicReadLine(const stBspAnlogIicPin *pin)
{
    if ((pin == NULL) || (pin->gpioPort == NULL)) {
        return false;
    }

    return (HAL_GPIO_ReadPin(pin->gpioPort, pin->gpioPin) == GPIO_PIN_SET);
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
