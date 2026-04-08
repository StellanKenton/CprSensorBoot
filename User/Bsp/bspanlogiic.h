#ifndef BSPANLOGIIC_H
#define BSPANLOGIIC_H

#include <stdbool.h>
#include <stdint.h>

#include "drvanlogiic.h"
#include "drvanlogiic_port.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

void bspAnlogIicInit(eDrvAnlogIicPortMap iic);
void bspAnlogIicSetScl(eDrvAnlogIicPortMap iic, bool releaseHigh);
void bspAnlogIicSetSda(eDrvAnlogIicPortMap iic, bool releaseHigh);
bool bspAnlogIicReadScl(eDrvAnlogIicPortMap iic);
bool bspAnlogIicReadSda(eDrvAnlogIicPortMap iic);
void bspAnlogIicDelayUs(uint16_t delayUs);

#ifdef __cplusplus
}
#endif

#endif  // BSPANLOGIIC_H
