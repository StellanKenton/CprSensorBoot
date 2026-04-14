/************************************************************************************
* @file     : drvgpio_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVGPIO_PORT_H
#define REBUILDCPR_DRVGPIO_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvGpioPinMap {
    DRVGPIO_EN_AUDIO = 0,
    DRVGPIO_RESET_WIFI,
    DRVGPIO_USB_SELECT,
    DRVGPIO_POWER_ON_CHECK,
} eDrvGpioPinMap;

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
