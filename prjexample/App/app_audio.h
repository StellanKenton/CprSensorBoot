/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_audio.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_AUDIO_H
#define APP_AUDIO_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

#include "drv_audio.h"
#define AUDIO_TASK_DELAY  20
typedef enum
{
	ADUIO_STRAT_CPR,
    ADUIO_DIDI,
    ADUIO_PRESS_DEEP,
    ADUIO_PRESS_SWALLOW,
	ADUIO_PRESS_SLOW,
    ADUIO_PRESS_FAST,  
    ADUIO_LOW_BATTERY,
    ADUIO_BATTERY_DEAD,
	ADUIO_CHANGE_LANGUAGE,
    ADUIO_PRESS_WELL,
    ADUIO_RELEASE_BAD,  
    AUDIO_MAX,
}Audio_List_EnumDef;

typedef enum
{
    AUDIO_ZH = 0x01,
    AUDIO_EN,
    AUDIO_DE,  
    AUDIO_FR,
    AUDIO_IT,
    AUDIO_Default = 0xFF,
    
}Audio_Language_EnumDef;

typedef enum
{
    AUDIO_PLAY_START,
    AUDIO_PLAY_DIDI,
    AUDIO_PLAY_NOTICE,
    
}Audio_Play_EnumDef;

void Audio_Modle_Init(void);
void App_Audio_Handle(void);

#ifdef __cplusplus
}
#endif
#endif  // APP_AUDIO_H
/**************************End of file********************************/

