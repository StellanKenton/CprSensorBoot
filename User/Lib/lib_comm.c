/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file lib_comm.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "lib_comm.h"



/**
* @brief key function for the library tools.
**/ 

uint16_t Crc16Compute_1234(const uint8_t *Data, uint16_t Length) 
{
    uint16_t Crc = 0xffff;              //!< initialize
    uint16_t Polynomial = 0x8005;       //!< polynomial：x^16 + x^15 + x^2 + 1

    for (uint16_t i = 0; i < Length; i++) 
    {
        Crc ^= (uint16_t)Data[i] << 8;
        
        for (uint8_t j = 0; j < 8; j++) 
        {
            if (Crc & 0x8000) 
            {
                Crc = (Crc << 1) ^ Polynomial;
            } 
            else 
            {
                Crc <<= 1;
            }
        }
    }
    
    return Crc;
}

uint16_t Crc16Compute(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0x0000;
    
    while (length--) {
        uint8_t b = *data++;
        
        // 输入反转（使用循环，节省代码空间）
        uint8_t r = 0;
        for (uint8_t i = 0; i < 8; i++) {
            r = (r << 1) | (b & 0x01);
            b >>= 1;
        }
        
        crc ^= (uint16_t)r << 8;
        
        // 处理8位
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc <<= 1;
            }
        }
    }
    
    // 输出反转（使用循环）
    uint16_t result = 0;
    for (uint8_t i = 0; i < 16; i++) {
        result = (result << 1) | (crc & 0x01);
        crc >>= 1;
    }
    
    return result;
}
/**************************End of file********************************/


