/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file lib_ringbuffer.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef LIB_RINGBUFFER_H
#define LIB_RINGBUFFER_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

typedef struct {
    uint8_t  HeadType;
    uint8_t  TailType;
    uint16_t HandleDataLength;
    uint16_t DataLen;
    uint8_t  HandleDataOverTime;
    uint8_t *pBuff;        
    uint32_t Size;         
    uint32_t Head;         
    uint32_t Tail;
    uint8_t *RevData;
    void (*unpack)(uint8_t *);   
} CBuff;


void CBuff_Init(CBuff *buffer, uint8_t *pBuff,uint8_t *RevBuff, uint32_t size);
uint32_t CBuff_GetLength(const CBuff *buffer);
bool CBuff_Write(CBuff *buffer, const uint8_t *data, uint32_t len);
bool CBuff_Pop(CBuff *buffer, uint8_t *data, uint32_t len);
bool CBuff_Read(CBuff *buffer, uint8_t *data, uint32_t len);
void CBuff_Clear(CBuff *buffer);
bool CBuff_IsEmpty(const CBuff *buffer);
bool CBuff_IsFull(const CBuff *buffer);
uint32_t CBuff_GetFreeSpace(const CBuff *buffer);

#ifdef __cplusplus
}
#endif
#endif  // LIB_RINGBUFFER_H
/**************************End of file********************************/

