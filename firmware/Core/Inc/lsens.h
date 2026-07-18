  /******************************************************************************
  * @file           : lsens.h
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/
#ifndef __LSENS_H
#define __LSENS_H

#include "main.h"

#define LSENS_READ_TIMES	    10
#define LSENS_MAX_ADC_VALUE     4095
#define LSENS_MAX_LIGHT_VALUE   100
 
uint8_t Lsens_Get_Val(void);				

#endif /* __LSENS_H */