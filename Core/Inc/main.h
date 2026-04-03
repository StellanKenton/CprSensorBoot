/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MCU_FORCE_ADC_Pin GPIO_PIN_0
#define MCU_FORCE_ADC_GPIO_Port GPIOC
#define MCU_DC_ADC_Pin GPIO_PIN_1
#define MCU_DC_ADC_GPIO_Port GPIOC
#define Power_ON_Check_Pin GPIO_PIN_2
#define Power_ON_Check_GPIO_Port GPIOC
#define MCU_BAT_ADC_Pin GPIO_PIN_0
#define MCU_BAT_ADC_GPIO_Port GPIOA
#define EN_AUDIO_Pin GPIO_PIN_1
#define EN_AUDIO_GPIO_Port GPIOA
#define SPI_CS_Pin GPIO_PIN_4
#define SPI_CS_GPIO_Port GPIOA
#define MCU_5V0_ADC_Pin GPIO_PIN_4
#define MCU_5V0_ADC_GPIO_Port GPIOC
#define MCU_3V3_ADC_Pin GPIO_PIN_5
#define MCU_3V3_ADC_GPIO_Port GPIOC
#define BAT_Charging_Status_Pin GPIO_PIN_13
#define BAT_Charging_Status_GPIO_Port GPIOB
#define BAT_ChargeDone_Status_Pin GPIO_PIN_14
#define BAT_ChargeDone_Status_GPIO_Port GPIOB
#define RESET_WIFI_Pin GPIO_PIN_15
#define RESET_WIFI_GPIO_Port GPIOB
#define USB_Select_Pin GPIO_PIN_8
#define USB_Select_GPIO_Port GPIOA
#define PCA9535_RESET_Pin GPIO_PIN_15
#define PCA9535_RESET_GPIO_Port GPIOA
#define MCU_LED_SDA_Pin GPIO_PIN_12
#define MCU_LED_SDA_GPIO_Port GPIOC
#define MCU_LED_CLK_Pin GPIO_PIN_2
#define MCU_LED_CLK_GPIO_Port GPIOD
#define PCA9535_SDA_Pin GPIO_PIN_3
#define PCA9535_SDA_GPIO_Port GPIOB
#define PCA9535_SCL_Pin GPIO_PIN_4
#define PCA9535_SCL_GPIO_Port GPIOB
#define Motor_PWM_Pin GPIO_PIN_5
#define Motor_PWM_GPIO_Port GPIOB
#define Buzzer_PWM_Pin GPIO_PIN_6
#define Buzzer_PWM_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
