  /******************************************************************************
  * @file           : lcd_disp.c
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/

#include "lcd_disp.h"
#include "lcd.h"

void lcd_disp_init(void)
{
    LCD_Init();
}

void lcd_disp_update(const char *date, const char *time, const SensorData_t *data)
{
    LCD_ShowxNum(30+10*8, 130, data->light_pct, 3, 24, 0);
    LCD_ShowString(70+10*8, 130, 10, 3, 24, "%");
    char buf[16];
    sprintf(buf, "%.1f", data->temp_c);
    LCD_ShowString(30+10*8, 200, 400, 3, 24, buf);
    LCD_ShowString(30+10*8, 270, 400, 3, 24, date);
    LCD_ShowString(30+10*8, 320, 400, 3, 24, time);
}