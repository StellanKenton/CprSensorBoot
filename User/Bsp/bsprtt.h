/************************************************************************************
* @file     : bsprtt.h
* @brief    : SEGGER RTT BSP adapter for log and console.
* @details  : Bridges the reusable log/console transport hooks to the board RTT link.
***********************************************************************************/
#ifndef BSPRTT_H
#define BSPRTT_H

#include <stdint.h>

#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BSP_RTT_LOG_OUTPUT_ENABLE
#define BSP_RTT_LOG_OUTPUT_ENABLE      1
#endif

#ifndef BSP_RTT_LOG_INPUT_ENABLE
#define BSP_RTT_LOG_INPUT_ENABLE       1
#endif

#ifndef BSP_RTT_UP_BUFFER_INDEX
#define BSP_RTT_UP_BUFFER_INDEX        0U
#endif

#ifndef BSP_RTT_DOWN_BUFFER_INDEX
#define BSP_RTT_DOWN_BUFFER_INDEX      0U
#endif

#ifndef BSP_RTT_INPUT_BUFFER_SIZE
#define BSP_RTT_INPUT_BUFFER_SIZE      256U
#endif

void bspRttLogInit(void);
int32_t bspRttLogWrite(const uint8_t *buffer, uint16_t length);
stRingBuffer *bspRttLogGetInputBuffer(void);

#ifdef __cplusplus
}
#endif

#endif  // BSPRTT_H
/**************************End of file********************************/
