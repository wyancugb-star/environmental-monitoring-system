  /******************************************************************************
  * @file           : lcd_disp.h
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/
#ifndef __LCD_DISP_H
#define __LCD_DISP_H

#include "main.h"
#include "acq_adc.h"

void lcd_disp_init(void);
void lcd_disp_update(const char *date, const char *time, const SensorData_t *data);

#endif /* __LCD_DISP_H */