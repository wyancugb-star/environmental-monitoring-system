/**
  ******************************************************************************
  * @file           : lcd.h
  * @brief          : TFT LCD driver (FSMC, NT35510 controller) - public interface
  *
  * @details
  * Drives a 4.3" TFT LCD (NT35510 controller) via STM32's FSMC
  * peripheral configured in "LCD Interface" mode: command/data are
  * distinguished by address line A6, mapped to LCD_REG / LCD_RAM below.
  *
  * Typical usage:
  *   1. LCD_Init() once at startup (runs the NT35510 register sequence,
  *      sets orientation, clears the screen)
  *   2. Use drawing functions (LCD_DrawPoint, LCD_Fill, LCD_ShowString, ...)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************
  */

#ifndef __LCD_H
#define __LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief LCD device state and geometry, initialized by LCD_Init()
 */
typedef struct  
{										    
	uint16_t width;     /**< Active display width in pixels */
  uint16_t height;    /**< Active display height in pixels */
  uint16_t id;        /**< LCD controller ID (0x5510 for NT35510) */
  uint8_t  dir;        /**< Orientation: 0 = portrait, 1 = landscape */
  uint16_t wramcmd;    /**< Register command to start GRAM write */
  uint16_t setxcmd;    /**< Register command to set X cursor position */
  uint16_t setycmd;    /**< Register command to set Y cursor position */
}_lcd_dev;

extern _lcd_dev lcddev;	

/* Current pen (foreground) and background colors used by drawing functions */
extern uint16_t  POINT_COLOR;//默认红色    
extern uint16_t  BACK_COLOR; //背景颜色.默认为白色

#define LCD_BL_ON()  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET)
#define LCD_BL_OFF() HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET)

/**
 * @brief FSMC-mapped LCD register access.
 *        LCD_REG / LCD_RAM correspond to FSMC address line A6 being
 *        low/high respectively (command vs. data/RAM access).
 */
typedef struct
{
	volatile uint16_t LCD_REG;
	volatile uint16_t LCD_RAM;
} LCD_TypeDef;

/* FSMC Bank1 NE4 base address, offset 0x7E selects A6 = 1 (LCD_RAM) */
#define LCD_BASE        ((uint32_t)(0x6C000000 | 0x0000007E))
#define LCD             ((LCD_TypeDef *) LCD_BASE)

/* GRAM scan direction options */
#define L2R_U2D  0  /* left-to-right, top-to-bottom */
#define L2R_D2U  1  /* left-to-right, bottom-to-top */
#define R2L_U2D  2  /* right-to-left, top-to-bottom */
#define R2L_D2U  3  /* right-to-left, bottom-to-top */
#define U2D_L2R  4  /* top-to-bottom, left-to-right */
#define U2D_R2L  5  /* top-to-bottom, right-to-left */
#define D2U_L2R  6  /* bottom-to-top, left-to-right */
#define D2U_R2L  7  /* bottom-to-top, right-to-left */

#define DFT_SCAN_DIR  L2R_U2D   /* Default scan direction */

/* Basic colors (RGB565) */
#define WHITE    0xFFFF
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define YELLOW   0xFFE0
#define CYAN     0x7FFF
#define MAGENTA  0xF81F
#define BROWN    0xBC40
#define GRAY     0x8430

/* GUI accent colors */
#define DARKBLUE   0x01CF
#define LIGHTBLUE  0x7D7C
#define GRAYBLUE   0x5458
#define LGRAY      0xC618  /* Panel background */
#define LGRAYBLUE  0xA651  /* Mid-layer color */
#define LBBLUE     0x2B12  /* Selected-item inverse color */

void LCD_Init(void);
void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue);
uint16_t LCD_ReadReg(uint16_t LCD_Reg);
void LCD_Clear(uint16_t color);
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos);
void LCD_DrawPoint(uint16_t x, uint16_t y);
void LCD_Fast_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
uint16_t LCD_ReadPoint(uint16_t x, uint16_t y);
void LCD_Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void LCD_Color_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode);
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size);
void LCD_ShowxNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode);
void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, const char *p);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H */