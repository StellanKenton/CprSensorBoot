/***********************************************************************************
* @file     : update.c
* @brief    : Update manager implementation.
* @details  : Keeps a minimal software-visible update state for later wiring.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update.h"

static stUpdateStatus gUpdateStatus;

void updateReset(void)
{
    gUpdateStatus.state = E_UPDATE_STATE_UNINIT;
    gUpdateStatus.isUpdateRequested = false;
    gUpdateStatus.lastProcessTick = 0U;
}

bool updateInit(void)
{
    updateReset();
    gUpdateStatus.state = E_UPDATE_STATE_IDLE;
    return true;
}


void updateProcess(uint32_t nowTick)
{
    gUpdateStatus.lastProcessTick = nowTick;

    if (gUpdateStatus.state == E_UPDATE_STATE_UNINIT) {
        return;
    }
}

const stUpdateStatus *updateGetStatus(void)
{
    return &gUpdateStatus;
}
/**************************End of file********************************/
