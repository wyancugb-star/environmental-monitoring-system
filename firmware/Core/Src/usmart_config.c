/**
  ******************************************************************************
  * @file           : usmart_config.c
  * @brief          : usmart function registration table
  *
  * @details
  * Defines which functions are callable from the UART terminal via
  * usmart. To expose a new function, add an entry to usmart_nametab[]
  * below, following the exact format:
  *   { (void*)function_pointer, "return_type function_name(param_types...)" }
  *
  * The string signature must match the actual function prototype
  * (return type, name, and parameter types), since usmart_str.c parses
  * this string to determine argument count/types when matching a
  * user-typed command against this table.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************
  */
#include "usmart.h"
#include "usmart_str.h"
#include "lcd.h"
#include "acq_adc.h"
#include "rtc_mgr.h"

/**
 * @brief  usmart-callable wrapper: clears the screen to a given color.
 *         Example: lcd_set_bg(0xF800)  -> red background
 */
void lcd_set_bg(uint16_t color)
{
    LCD_Clear(color);
}

/**
 * @brief  usmart-callable wrapper: sets the current pen (text/draw) color.
 *         Example: lcd_set_color(0x001F)  -> subsequent text drawn in blue
 */
void lcd_set_color(uint16_t color)
{
    POINT_COLOR = color;
}

/**
 * @brief  Table of functions exposed to the usmart command interface.
 * @note   Add new entries here to make additional functions callable
 *         from the UART terminal (e.g. typing "led_set(1)").
 */
const usmart_func_t usmart_nametab[] =
{
    {(void*)LCD_Clear, "void LCD_Clear(uint16_t Color)"},
    {(void*)lcd_set_bg,    "void lcd_set_bg(uint16_t color)"},
    {(void*)lcd_set_color, "void lcd_set_color(uint16_t color)"},
    {(void*)light_adc_raw_print, "void light_adc_raw_print(void)"},
    {(void*)temp_adc_raw_print, "void temp_adc_raw_print(void)"},
    {(void*)set_rtc_time, "void set_rtc_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)"},
};					  

/**
 * @brief  usmart runtime instance, wiring the function table and
 *         core routines (defined in usmart.c) together.
 */
usmart_t usmart_dev =
{
    .funs     = usmart_nametab,
    .init     = usmart_init,
    .cmd_rec  = usmart_cmd_rec,
    .exe      = usmart_exe,

    .fnum     = sizeof(usmart_nametab) / sizeof(usmart_func_t),
    .pnum     = 0,
    .id       = 0,

    .parmtype = 0,

    .plentbl  = {0},
    .parm     = {0},
};
