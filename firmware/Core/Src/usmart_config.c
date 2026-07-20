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
 * @brief  usmart-callable wrapper: displays a string at the given position.
 * @note   Registered with usmart so it can be called from the UART
 *         terminal, e.g.: lcd_show_text(50,100,24,"Hello")
 */
void lcd_show_text(uint16_t x, uint16_t y, uint8_t size, char *text)
{
    LCD_ShowString(x, y, 400, size + 4, size, text);
}

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
 * @brief  usmart-callable wrapper: draws a filled rectangle.
 *         Example: lcd_draw_box(50,50,200,150,0x07E0)
 */
void lcd_draw_box(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_Fill(x1, y1, x2, y2, color);
}

/**
 * @brief  usmart-callable wrapper: displays a decimal number.
 *         Example: lcd_show_num(50,200,16,12345)
 */
void lcd_show_num(uint16_t x, uint16_t y, uint8_t size, uint32_t num)
{
    LCD_ShowNum(x, y, num, 8, size);
}

/**
 * @brief  Table of functions exposed to the usmart command interface.
 * @note   Add new entries here to make additional functions callable
 *         from the UART terminal (e.g. typing "led_set(1)").
 */
const usmart_func_t usmart_nametab[] =
{
    {(void*)LCD_Clear, "void LCD_Clear(uint16_t Color)"},
    {(void*)lcd_show_text, "void lcd_show_text(uint16_t x,uint16_t y,uint8_t size,char *text)"},
    {(void*)lcd_set_bg,    "void lcd_set_bg(uint16_t color)"},
    {(void*)lcd_set_color, "void lcd_set_color(uint16_t color)"},
    {(void*)lcd_draw_box,  "void lcd_draw_box(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t color)"},
    {(void*)lcd_show_num,  "void lcd_show_num(uint16_t x,uint16_t y,uint8_t size,uint32_t num)"},
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
