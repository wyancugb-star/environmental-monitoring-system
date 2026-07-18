  /******************************************************************************
  * @file           : lsens.c
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/

#include "lsens.h"
#include "adc.h"

/**
 * @brief  Read the ambient light intensity.
 * @retval Light intensity (0~100)
 *         - 0   : Darkest
 *         - 100 : Brightest
 */
uint8_t Lsens_Get_Val(void)
{
	uint32_t adcSum = 0;

    for (uint8_t i = 0; i < LSENS_READ_TIMES; i++)
    {
        HAL_ADC_Start(&hadc3);
        HAL_ADC_PollForConversion(&hadc3, 10);
        adcSum += HAL_ADC_GetValue(&hadc3);
        HAL_ADC_Stop(&hadc3);

        HAL_Delay(5);
    }

    uint32_t adcValue = adcSum / LSENS_READ_TIMES;

    if (adcValue > LSENS_MAX_ADC_VALUE)
    {
        adcValue = LSENS_MAX_ADC_VALUE;
    }

    return (uint8_t)(LSENS_MAX_LIGHT_VALUE - (adcValue * LSENS_MAX_LIGHT_VALUE) / LSENS_MAX_ADC_VALUE);
}

