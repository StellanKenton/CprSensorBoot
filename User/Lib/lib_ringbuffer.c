/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file lib_ringbuffer.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "lib_ringbuffer.h"


/**
* @brief Init the circular buffer.
* @param buffer: Pointer to the circular buffer structure.
* @param pBuff: Pointer to the buffer memory.
* @param size: Size of the buffer.
* @return None.
**/
void CBuff_Init(CBuff *buffer, uint8_t *pBuff,uint8_t *RevBuff, uint32_t size)
{
    buffer->pBuff = pBuff;
    buffer->Size = size;
    buffer->Head = 0;
    buffer->Tail = 0;
    buffer->RevData = RevBuff;
}

/**
* @brief Get the current length of the circular buffer.
* @param buffer: Pointer to the circular buffer structure.
* @return The number of elements currently in the buffer.
**/
uint32_t CBuff_GetLength(const CBuff *buffer)
{
    return (buffer->Head >= buffer->Tail) ? (buffer->Head - buffer->Tail) : (buffer->Size + buffer->Head - buffer->Tail);
}

/**
* @brief Write data to the circular buffer.
* @param buffer: Pointer to the circular buffer structure.
* @param data: Pointer to the data to write.
* @param len: Length of the data to write.
* @return true if the data was written successfully, false if there is not enough space.
**/
bool CBuff_Write(CBuff *buffer, const uint8_t *data, uint32_t len)
{
    uint32_t freeSpace = buffer->Size - CBuff_GetLength(buffer) - 1; 
    uint32_t Head,i;

    if (freeSpace < len)
    {
        return false; 
    }

    Head = buffer->Head;
    for (i = 0; i < len; i++)
    {
        buffer->pBuff[Head] = data[i];
        Head = (Head + 1) % buffer->Size;
    }
    buffer->Head = Head;

    return true; 
}
/**
* @brief Read data from the circular buffer without removing it.
* @param buffer: Pointer to the circular buffer structure.
* @param data: Pointer to the buffer where data will be read into.
* @param len: Length of the data to read.
* @return true if the data was read successfully, false if there is not enough data.
**/
bool CBuff_Read(CBuff *buffer, uint8_t *data, uint32_t len)
{
    uint32_t availableData = CBuff_GetLength(buffer);
    uint32_t Tail,i;

    if (availableData < len)
    {
        return false; 
    }
    
    Tail = buffer->Tail;
    for (i = 0; i < len; i++)
    {
        data[i] = buffer->pBuff[Tail];
		Tail = (Tail + 1) % buffer->Size;
    }
    
    return true; 
}
/**
* @brief Pop data from the circular buffer, removing it from the buffer.
* @param buffer: Pointer to the circular buffer structure.
* @param data: Pointer to the buffer where data will be popped into.
* @param len: Length of the data to pop.
* @return true if the data was popped successfully, false if there is not enough data.
**/
bool CBuff_Pop(CBuff *buffer, uint8_t *data, uint32_t len)
{
    uint32_t availableData = CBuff_GetLength(buffer);
    uint32_t Tail,i;

    if (availableData < len)
    {
        return false; 
    }
    
    Tail = buffer->Tail;
    for (i = 0; i < len; i++)
    {
        data[i] = buffer->pBuff[Tail];
        Tail = (Tail + 1) % buffer->Size;
    }
    buffer->Tail = Tail;
    return true; 
}

/**
* @brief Clear the circular buffer.
* @param buffer: Pointer to the circular buffer structure.
* @return None. 
**/
void CBuff_Clear(CBuff *buffer)
{
    buffer->Head = 0;
    buffer->Tail = 0;
}

/**
* @brief Check if the circular buffer is empty.
* @param buffer: Pointer to the circular buffer structure.
* @return true if the buffer is empty, false otherwise. 
**/
bool CBuff_IsEmpty(const CBuff *buffer)
{
    return (buffer->Head == buffer->Tail);
}

/**
* @brief Check if the circular buffer is full.
* @param buffer: Pointer to the circular buffer structure.
* @return true if the buffer is full, false otherwise.
**/
bool CBuff_IsFull(const CBuff *buffer)
{
    return ((buffer->Head + 1) % buffer->Size) == buffer->Tail;
}

/**
* @brief Get the amount of free space in the circular buffer.
* @param buffer: Pointer to the circular buffer structure.
* @return The number of free bytes available in the buffer.
**/
uint32_t CBuff_GetFreeSpace(const CBuff *buffer)
{
    return buffer->Size - CBuff_GetLength(buffer) - 1; 
}

/**************************End of file********************************/


