/**
  ******************************************************************************
  * @file           : lcd.c
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************
  */

#include "lcd.h"
#include "font.h"

/* Current pen (foreground) and background color used by drawing functions */
uint16_t POINT_COLOR=0x0000;	
uint16_t BACK_COLOR=0xFFFF;  

_lcd_dev lcddev;

/**
 * @brief  Writes a command/register index to the LCD.
 * @note   The `regval=regval` line is a no-op kept to prevent the
 *         compiler from over-optimizing away the FSMC write timing
 *         under -O2 (see also LCD_WR_DATA, LCD_RD_DATA).
 */
void LCD_WR_REG(volatile uint16_t regval)
{   
	regval=regval;		
	LCD->LCD_REG=regval;	 
}

/**
 * @brief  Writes a data value (GRAM/parameter) to the LCD.
 */
void LCD_WR_DATA(volatile uint16_t data)
{	  
	data=data;			
	LCD->LCD_RAM=data;		 
}

/**
 * @brief  Reads the current LCD data register value.
 */
uint16_t LCD_RD_DATA(void)
{
	volatile uint16_t ram;			
	ram=LCD->LCD_RAM;	
	return ram;	 
}	

/**
 * @brief  Writes a register index followed by its value in one call.
 */
void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue)
{	
	LCD->LCD_REG = LCD_Reg;		
	LCD->LCD_RAM = LCD_RegValue;	    		 
}	   

/**
 * @brief  Reads back the value of a given LCD register.
 */
uint16_t LCD_ReadReg(uint16_t LCD_Reg)
{										   
	LCD_WR_REG(LCD_Reg);		
	for (volatile int i = 0; i < 100; i++);	  
	return LCD_RD_DATA();		
}   

/**
 * @brief  Sends the "start GRAM write" command, preparing the LCD to
 *         accept a stream of pixel data at the current cursor position.
 */
void LCD_WriteRAM_Prepare(void)
{
 	LCD->LCD_REG=lcddev.wramcmd;	  
}	 

/**
 * @brief  Writes one 16-bit pixel color directly to GRAM.
 * @note   Caller must have already called LCD_WriteRAM_Prepare() (or
 *         LCD_SetCursor + LCD_WriteRAM_Prepare) beforehand.
 */
void LCD_WriteRAM(uint16_t RGB_Code)
{							    
	LCD->LCD_RAM = RGB_Code;
}

/**
 * @brief  Converts a BGR565 color value to RGB565 format.
 */
uint16_t LCD_BGR2RGB(uint16_t c)
{
	uint16_t  r,g,b,rgb;   
	b=(c>>0)&0x1f;
	g=(c>>5)&0x3f;
	r=(c>>11)&0x1f;	 
	rgb=(b<<11)+(g<<5)+(r<<0);		 
	return(rgb);
} 

/**
 * @brief  Short busy-wait delay used for read-timing compliance on
 *         certain controllers, kept in case optimization removes
 *         normal delay calls.
 */
void opt_delay(uint8_t i)
{
	while(i--);
}

/**
 * @brief  Reads back the pixel color at the given coordinates.
 * @note   NT35510 read-back requires two transfers: the first returns
 *         R+G packed together, the second returns B.
 */
uint16_t LCD_ReadPoint(uint16_t x,uint16_t y)
{
 	uint16_t r=0,g=0,b=0;
	if(x>=lcddev.width||y>=lcddev.height)return 0;

	LCD_SetCursor(x,y);	    

	LCD_WR_REG(0X2E00);			// NT35510 read-GRAM command 
 	r=LCD_RD_DATA();					// dummy Read	   
	opt_delay(2);	  
 	r=LCD_RD_DATA();  		  			// R + G packed 
	
	opt_delay(2);	  
	b=LCD_RD_DATA(); 
	g=r&0XFF;							// B
	g<<=8;

	return (((r>>11)<<11)|((g>>10)<<5)|(b>>11));
}			 

/**
 * @brief  Turns the LCD panel display on.
 */
void LCD_DisplayOn(void)
{					   
    LCD_WR_REG(0X2900);	
}	 

/**
 * @brief  Turns the LCD panel display off (blank screen, backlight unaffected).
 */
void LCD_DisplayOff(void)
{	   
	LCD_WR_REG(0X2800);
}   

/**
 * @brief  Sets the GRAM write cursor to the given (X, Y) coordinates.
 */
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
{	
	LCD_WR_REG(lcddev.setxcmd);		LCD_WR_DATA(Xpos >> 8); 		
	LCD_WR_REG(lcddev.setxcmd + 1);	LCD_WR_DATA(Xpos & 0XFF);			 
	LCD_WR_REG(lcddev.setycmd);		LCD_WR_DATA(Ypos >> 8);  		
	LCD_WR_REG(lcddev.setycmd + 1);	LCD_WR_DATA(Ypos & 0XFF);				 
} 		

/**
 * @brief  Configures the GRAM auto-scan direction and updates the
 *         active drawing window to match.
 * @note   Non-default directions can occasionally cause display glitches
 *         on some controllers; L2R_U2D (default) is the safest choice.
 * @param  dir  Scan direction, one of the L2R_U2D / R2L_D2U / ... macros
 */   	   
void LCD_Scan_Dir(uint8_t dir)
{
	uint16_t regval=0;
	uint16_t temp;  		   
	
	switch(dir)
	{
		case L2R_U2D: regval |= (0<<7)|(0<<6)|(0<<5); break;
		case L2R_D2U: regval |= (1<<7)|(0<<6)|(0<<5); break;
		case R2L_U2D: regval |= (0<<7)|(1<<6)|(0<<5); break;
		case R2L_D2U: regval |= (1<<7)|(1<<6)|(0<<5); break;	 
		case U2D_L2R: regval |= (0<<7)|(0<<6)|(1<<5); break;
		case U2D_R2L: regval |= (0<<7)|(1<<6)|(1<<5); break;
		case D2U_L2R: regval |= (1<<7)|(0<<6)|(1<<5); break;
		case D2U_R2L: regval |= (1<<7)|(1<<6)|(1<<5); break;	 
	}

	LCD_WriteReg(0x3600, regval);   /* NT35510 memory access control register */

	/* Bit 5 (MV) swaps X/Y addressing; keep width/height consistent with it */
	if(regval & 0X20)
	{
		if(lcddev.width < lcddev.height)
		{
			temp = lcddev.width;
			lcddev.width = lcddev.height;
			lcddev.height = temp;
		}
	}
	else  
	{
		if(lcddev.width > lcddev.height)
		{
			temp = lcddev.width;
			lcddev.width = lcddev.height;
			lcddev.height = temp;
		}
	}  

	/* Reset the active drawing window to the full screen */
	LCD_WR_REG(lcddev.setxcmd);     LCD_WR_DATA(0);
	LCD_WR_REG(lcddev.setxcmd + 1); LCD_WR_DATA(0);
	LCD_WR_REG(lcddev.setxcmd + 2); LCD_WR_DATA((lcddev.width - 1) >> 8);
	LCD_WR_REG(lcddev.setxcmd + 3); LCD_WR_DATA((lcddev.width - 1) & 0xFF);
	LCD_WR_REG(lcddev.setycmd);     LCD_WR_DATA(0);
	LCD_WR_REG(lcddev.setycmd + 1); LCD_WR_DATA(0);
	LCD_WR_REG(lcddev.setycmd + 2); LCD_WR_DATA((lcddev.height - 1) >> 8);
	LCD_WR_REG(lcddev.setycmd + 3); LCD_WR_DATA((lcddev.height - 1) & 0xFF);

}     

/**
 * @brief  Draws a single pixel at (x, y) using the current POINT_COLOR.
 */
void LCD_DrawPoint(uint16_t x,uint16_t y)
{
	LCD_SetCursor(x,y);		
	LCD_WriteRAM_Prepare();	
	LCD->LCD_RAM = POINT_COLOR; 
}

/**
 * @brief  Draws a single pixel at (x, y) with an explicit color,
 *         bypassing LCD_SetCursor for slightly lower overhead.
 */
void LCD_Fast_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{	   
	LCD_WR_REG(lcddev.setxcmd);     LCD_WR_DATA(x >> 8);
    LCD_WR_REG(lcddev.setxcmd + 1); LCD_WR_DATA(x & 0xFF);
    LCD_WR_REG(lcddev.setycmd);     LCD_WR_DATA(y >> 8);
    LCD_WR_REG(lcddev.setycmd + 1); LCD_WR_DATA(y & 0xFF);
    LCD->LCD_REG = lcddev.wramcmd;
    LCD->LCD_RAM = color;
}	 

/**
 * @brief  Sets the display orientation and the corresponding
 *         active resolution (800x480 landscape / 480x800 portrait).
 * @param  dir  0 = portrait, 1 = landscape
 */
void LCD_Display_Dir(uint8_t dir)
{
	lcddev.dir = dir;
    lcddev.wramcmd = 0x2C00;
    lcddev.setxcmd = 0x2A00;
    lcddev.setycmd = 0x2B00;

    if (dir == 0)   /* portrait */
    {
        lcddev.width  = 480;
        lcddev.height = 800;
    }
    else            /* landscape */
    {
        lcddev.width  = 800;
        lcddev.height = 480;
    }

    LCD_Scan_Dir(DFT_SCAN_DIR);
}	 

/**
 * @brief  Restricts subsequent GRAM writes to a rectangular window,
 *         with the write cursor positioned at its top-left corner.
 * @param  sx, sy      Top-left corner coordinates
 * @param  width, height  Window size in pixels (must be > 0)
 */
void LCD_Set_Window(uint16_t sx,uint16_t sy,uint16_t width,uint16_t height)
{    
	uint16_t twidth = sx + width - 1;
    uint16_t theight = sy + height - 1;

    LCD_WR_REG(lcddev.setxcmd);     LCD_WR_DATA(sx >> 8);
    LCD_WR_REG(lcddev.setxcmd + 1); LCD_WR_DATA(sx & 0xFF);
    LCD_WR_REG(lcddev.setxcmd + 2); LCD_WR_DATA(twidth >> 8);
    LCD_WR_REG(lcddev.setxcmd + 3); LCD_WR_DATA(twidth & 0xFF);
    LCD_WR_REG(lcddev.setycmd);     LCD_WR_DATA(sy >> 8);
    LCD_WR_REG(lcddev.setycmd + 1); LCD_WR_DATA(sy & 0xFF);
    LCD_WR_REG(lcddev.setycmd + 2); LCD_WR_DATA(theight >> 8);
    LCD_WR_REG(lcddev.setycmd + 3); LCD_WR_DATA(theight & 0xFF);
}

/**
 * @brief  Initializes the NT35510 LCD controller.
 *
 * @details
 * Runs the manufacturer-recommended power-on register sequence:
 *   - Unlocks extended command pages (0xF000-0xF004)
 *   - Configures analog power supplies (AVDD/AVEE/VCL/VGH/VGL/VCOM)
 *   - Loads the gamma correction lookup tables (0xD1xx-0xD6xx, 6 gamma
 *     curves for RGB sub-pixel driving)
 *   - Configures display timing/EQ/inversion settings
 *   - Sets pixel format to 16-bit/pixel (RGB565) and exits sleep mode
 *
 * These register values are copied from the panel manufacturer's
 * reference initialization sequence and should not be modified
 * without datasheet guidance.
 *
 * After the register sequence completes, this function also sets
 * the default orientation, turns on the backlight, and clears the
 * screen to white.
 */
void LCD_Init(void)
{
    LCD_WriteReg(0xF000,0x55);
	LCD_WriteReg(0xF001,0xAA);
	LCD_WriteReg(0xF002,0x52);
	LCD_WriteReg(0xF003,0x08);
	LCD_WriteReg(0xF004,0x01);
	//AVDD Set AVDD 5.2V
	LCD_WriteReg(0xB000,0x0D);
	LCD_WriteReg(0xB001,0x0D);
	LCD_WriteReg(0xB002,0x0D);
	//AVDD ratio
	LCD_WriteReg(0xB600,0x34);
	LCD_WriteReg(0xB601,0x34);
	LCD_WriteReg(0xB602,0x34);
	//AVEE -5.2V
	LCD_WriteReg(0xB100,0x0D);
	LCD_WriteReg(0xB101,0x0D);
	LCD_WriteReg(0xB102,0x0D);
	//AVEE ratio
	LCD_WriteReg(0xB700,0x34);
	LCD_WriteReg(0xB701,0x34);
	LCD_WriteReg(0xB702,0x34);
	//VCL -2.5V
	LCD_WriteReg(0xB200,0x00);
	LCD_WriteReg(0xB201,0x00);
	LCD_WriteReg(0xB202,0x00);
	//VCL ratio
	LCD_WriteReg(0xB800,0x24);
	LCD_WriteReg(0xB801,0x24);
	LCD_WriteReg(0xB802,0x24);
	//VGH 15V (Free pump)
	LCD_WriteReg(0xBF00,0x01);
	LCD_WriteReg(0xB300,0x0F);
	LCD_WriteReg(0xB301,0x0F);
	LCD_WriteReg(0xB302,0x0F);
	//VGH ratio
	LCD_WriteReg(0xB900,0x34);
	LCD_WriteReg(0xB901,0x34);
	LCD_WriteReg(0xB902,0x34);
	//VGL_REG -10V
	LCD_WriteReg(0xB500,0x08);
	LCD_WriteReg(0xB501,0x08);
	LCD_WriteReg(0xB502,0x08);
	LCD_WriteReg(0xC200,0x03);
	//VGLX ratio
	LCD_WriteReg(0xBA00,0x24);
	LCD_WriteReg(0xBA01,0x24);
	LCD_WriteReg(0xBA02,0x24);
	//VGMP/VGSP 4.5V/0V
	LCD_WriteReg(0xBC00,0x00);
	LCD_WriteReg(0xBC01,0x78);
	LCD_WriteReg(0xBC02,0x00);
	//VGMN/VGSN -4.5V/0V
	LCD_WriteReg(0xBD00,0x00);
	LCD_WriteReg(0xBD01,0x78);
	LCD_WriteReg(0xBD02,0x00);
	//VCOM
	LCD_WriteReg(0xBE00,0x00);
	LCD_WriteReg(0xBE01,0x64);
	//Gamma Setting
	LCD_WriteReg(0xD100,0x00);
	LCD_WriteReg(0xD101,0x33);
	LCD_WriteReg(0xD102,0x00);
	LCD_WriteReg(0xD103,0x34);
	LCD_WriteReg(0xD104,0x00);
	LCD_WriteReg(0xD105,0x3A);
	LCD_WriteReg(0xD106,0x00);
	LCD_WriteReg(0xD107,0x4A);
	LCD_WriteReg(0xD108,0x00);
	LCD_WriteReg(0xD109,0x5C);
	LCD_WriteReg(0xD10A,0x00);
	LCD_WriteReg(0xD10B,0x81);
	LCD_WriteReg(0xD10C,0x00);
	LCD_WriteReg(0xD10D,0xA6);
	LCD_WriteReg(0xD10E,0x00);
	LCD_WriteReg(0xD10F,0xE5);
	LCD_WriteReg(0xD110,0x01);
	LCD_WriteReg(0xD111,0x13);
	LCD_WriteReg(0xD112,0x01);
	LCD_WriteReg(0xD113,0x54);
	LCD_WriteReg(0xD114,0x01);
	LCD_WriteReg(0xD115,0x82);
	LCD_WriteReg(0xD116,0x01);
	LCD_WriteReg(0xD117,0xCA);
	LCD_WriteReg(0xD118,0x02);
	LCD_WriteReg(0xD119,0x00);
	LCD_WriteReg(0xD11A,0x02);
	LCD_WriteReg(0xD11B,0x01);
	LCD_WriteReg(0xD11C,0x02);
	LCD_WriteReg(0xD11D,0x34);
	LCD_WriteReg(0xD11E,0x02);
	LCD_WriteReg(0xD11F,0x67);
	LCD_WriteReg(0xD120,0x02);
	LCD_WriteReg(0xD121,0x84);
	LCD_WriteReg(0xD122,0x02);
	LCD_WriteReg(0xD123,0xA4);
	LCD_WriteReg(0xD124,0x02);
	LCD_WriteReg(0xD125,0xB7);
	LCD_WriteReg(0xD126,0x02);
	LCD_WriteReg(0xD127,0xCF);
	LCD_WriteReg(0xD128,0x02);
	LCD_WriteReg(0xD129,0xDE);
	LCD_WriteReg(0xD12A,0x02);
	LCD_WriteReg(0xD12B,0xF2);
	LCD_WriteReg(0xD12C,0x02);
	LCD_WriteReg(0xD12D,0xFE);
	LCD_WriteReg(0xD12E,0x03);
	LCD_WriteReg(0xD12F,0x10);
	LCD_WriteReg(0xD130,0x03);
	LCD_WriteReg(0xD131,0x33);
	LCD_WriteReg(0xD132,0x03);
	LCD_WriteReg(0xD133,0x6D);
	LCD_WriteReg(0xD200,0x00);
	LCD_WriteReg(0xD201,0x33);
	LCD_WriteReg(0xD202,0x00);
	LCD_WriteReg(0xD203,0x34);
	LCD_WriteReg(0xD204,0x00);
	LCD_WriteReg(0xD205,0x3A);
	LCD_WriteReg(0xD206,0x00);
	LCD_WriteReg(0xD207,0x4A);
	LCD_WriteReg(0xD208,0x00);
	LCD_WriteReg(0xD209,0x5C);
	LCD_WriteReg(0xD20A,0x00);

	LCD_WriteReg(0xD20B,0x81);
	LCD_WriteReg(0xD20C,0x00);
	LCD_WriteReg(0xD20D,0xA6);
	LCD_WriteReg(0xD20E,0x00);
	LCD_WriteReg(0xD20F,0xE5);
	LCD_WriteReg(0xD210,0x01);
	LCD_WriteReg(0xD211,0x13);
	LCD_WriteReg(0xD212,0x01);
	LCD_WriteReg(0xD213,0x54);
	LCD_WriteReg(0xD214,0x01);
	LCD_WriteReg(0xD215,0x82);
	LCD_WriteReg(0xD216,0x01);
	LCD_WriteReg(0xD217,0xCA);
	LCD_WriteReg(0xD218,0x02);
	LCD_WriteReg(0xD219,0x00);
	LCD_WriteReg(0xD21A,0x02);
	LCD_WriteReg(0xD21B,0x01);
	LCD_WriteReg(0xD21C,0x02);
	LCD_WriteReg(0xD21D,0x34);
	LCD_WriteReg(0xD21E,0x02);
	LCD_WriteReg(0xD21F,0x67);
	LCD_WriteReg(0xD220,0x02);
	LCD_WriteReg(0xD221,0x84);
	LCD_WriteReg(0xD222,0x02);
	LCD_WriteReg(0xD223,0xA4);
	LCD_WriteReg(0xD224,0x02);
	LCD_WriteReg(0xD225,0xB7);
	LCD_WriteReg(0xD226,0x02);
	LCD_WriteReg(0xD227,0xCF);
	LCD_WriteReg(0xD228,0x02);
	LCD_WriteReg(0xD229,0xDE);
	LCD_WriteReg(0xD22A,0x02);
	LCD_WriteReg(0xD22B,0xF2);
	LCD_WriteReg(0xD22C,0x02);
	LCD_WriteReg(0xD22D,0xFE);
	LCD_WriteReg(0xD22E,0x03);
	LCD_WriteReg(0xD22F,0x10);
	LCD_WriteReg(0xD230,0x03);
	LCD_WriteReg(0xD231,0x33);
	LCD_WriteReg(0xD232,0x03);
	LCD_WriteReg(0xD233,0x6D);
	LCD_WriteReg(0xD300,0x00);
	LCD_WriteReg(0xD301,0x33);
	LCD_WriteReg(0xD302,0x00);
	LCD_WriteReg(0xD303,0x34);
	LCD_WriteReg(0xD304,0x00);
	LCD_WriteReg(0xD305,0x3A);
	LCD_WriteReg(0xD306,0x00);
	LCD_WriteReg(0xD307,0x4A);
	LCD_WriteReg(0xD308,0x00);
	LCD_WriteReg(0xD309,0x5C);
	LCD_WriteReg(0xD30A,0x00);

	LCD_WriteReg(0xD30B,0x81);
	LCD_WriteReg(0xD30C,0x00);
	LCD_WriteReg(0xD30D,0xA6);
	LCD_WriteReg(0xD30E,0x00);
	LCD_WriteReg(0xD30F,0xE5);
	LCD_WriteReg(0xD310,0x01);
	LCD_WriteReg(0xD311,0x13);
	LCD_WriteReg(0xD312,0x01);
	LCD_WriteReg(0xD313,0x54);
	LCD_WriteReg(0xD314,0x01);
	LCD_WriteReg(0xD315,0x82);
	LCD_WriteReg(0xD316,0x01);
	LCD_WriteReg(0xD317,0xCA);
	LCD_WriteReg(0xD318,0x02);
	LCD_WriteReg(0xD319,0x00);
	LCD_WriteReg(0xD31A,0x02);
	LCD_WriteReg(0xD31B,0x01);
	LCD_WriteReg(0xD31C,0x02);
	LCD_WriteReg(0xD31D,0x34);
	LCD_WriteReg(0xD31E,0x02);
	LCD_WriteReg(0xD31F,0x67);
	LCD_WriteReg(0xD320,0x02);
	LCD_WriteReg(0xD321,0x84);
	LCD_WriteReg(0xD322,0x02);
	LCD_WriteReg(0xD323,0xA4);
	LCD_WriteReg(0xD324,0x02);
	LCD_WriteReg(0xD325,0xB7);
	LCD_WriteReg(0xD326,0x02);
	LCD_WriteReg(0xD327,0xCF);
	LCD_WriteReg(0xD328,0x02);
	LCD_WriteReg(0xD329,0xDE);
	LCD_WriteReg(0xD32A,0x02);
	LCD_WriteReg(0xD32B,0xF2);
	LCD_WriteReg(0xD32C,0x02);
	LCD_WriteReg(0xD32D,0xFE);
	LCD_WriteReg(0xD32E,0x03);
	LCD_WriteReg(0xD32F,0x10);
	LCD_WriteReg(0xD330,0x03);
	LCD_WriteReg(0xD331,0x33);
	LCD_WriteReg(0xD332,0x03);
	LCD_WriteReg(0xD333,0x6D);
	LCD_WriteReg(0xD400,0x00);
	LCD_WriteReg(0xD401,0x33);
	LCD_WriteReg(0xD402,0x00);
	LCD_WriteReg(0xD403,0x34);
	LCD_WriteReg(0xD404,0x00);
	LCD_WriteReg(0xD405,0x3A);
	LCD_WriteReg(0xD406,0x00);
	LCD_WriteReg(0xD407,0x4A);
	LCD_WriteReg(0xD408,0x00);
	LCD_WriteReg(0xD409,0x5C);
	LCD_WriteReg(0xD40A,0x00);
	LCD_WriteReg(0xD40B,0x81);

	LCD_WriteReg(0xD40C,0x00);
	LCD_WriteReg(0xD40D,0xA6);
	LCD_WriteReg(0xD40E,0x00);
	LCD_WriteReg(0xD40F,0xE5);
	LCD_WriteReg(0xD410,0x01);
	LCD_WriteReg(0xD411,0x13);
	LCD_WriteReg(0xD412,0x01);
	LCD_WriteReg(0xD413,0x54);
	LCD_WriteReg(0xD414,0x01);
	LCD_WriteReg(0xD415,0x82);
	LCD_WriteReg(0xD416,0x01);
	LCD_WriteReg(0xD417,0xCA);
	LCD_WriteReg(0xD418,0x02);
	LCD_WriteReg(0xD419,0x00);
	LCD_WriteReg(0xD41A,0x02);
	LCD_WriteReg(0xD41B,0x01);
	LCD_WriteReg(0xD41C,0x02);
	LCD_WriteReg(0xD41D,0x34);
	LCD_WriteReg(0xD41E,0x02);
	LCD_WriteReg(0xD41F,0x67);
	LCD_WriteReg(0xD420,0x02);
	LCD_WriteReg(0xD421,0x84);
	LCD_WriteReg(0xD422,0x02);
	LCD_WriteReg(0xD423,0xA4);
	LCD_WriteReg(0xD424,0x02);
	LCD_WriteReg(0xD425,0xB7);
	LCD_WriteReg(0xD426,0x02);
	LCD_WriteReg(0xD427,0xCF);
	LCD_WriteReg(0xD428,0x02);
	LCD_WriteReg(0xD429,0xDE);
	LCD_WriteReg(0xD42A,0x02);
	LCD_WriteReg(0xD42B,0xF2);
	LCD_WriteReg(0xD42C,0x02);
	LCD_WriteReg(0xD42D,0xFE);
	LCD_WriteReg(0xD42E,0x03);
	LCD_WriteReg(0xD42F,0x10);
	LCD_WriteReg(0xD430,0x03);
	LCD_WriteReg(0xD431,0x33);
	LCD_WriteReg(0xD432,0x03);
	LCD_WriteReg(0xD433,0x6D);
	LCD_WriteReg(0xD500,0x00);
	LCD_WriteReg(0xD501,0x33);
	LCD_WriteReg(0xD502,0x00);
	LCD_WriteReg(0xD503,0x34);
	LCD_WriteReg(0xD504,0x00);
	LCD_WriteReg(0xD505,0x3A);
	LCD_WriteReg(0xD506,0x00);
	LCD_WriteReg(0xD507,0x4A);
	LCD_WriteReg(0xD508,0x00);
	LCD_WriteReg(0xD509,0x5C);
	LCD_WriteReg(0xD50A,0x00);
	LCD_WriteReg(0xD50B,0x81);

	LCD_WriteReg(0xD50C,0x00);
	LCD_WriteReg(0xD50D,0xA6);
	LCD_WriteReg(0xD50E,0x00);
	LCD_WriteReg(0xD50F,0xE5);
	LCD_WriteReg(0xD510,0x01);
	LCD_WriteReg(0xD511,0x13);
	LCD_WriteReg(0xD512,0x01);
	LCD_WriteReg(0xD513,0x54);
	LCD_WriteReg(0xD514,0x01);
	LCD_WriteReg(0xD515,0x82);
	LCD_WriteReg(0xD516,0x01);
	LCD_WriteReg(0xD517,0xCA);
	LCD_WriteReg(0xD518,0x02);
	LCD_WriteReg(0xD519,0x00);
	LCD_WriteReg(0xD51A,0x02);
	LCD_WriteReg(0xD51B,0x01);
	LCD_WriteReg(0xD51C,0x02);
	LCD_WriteReg(0xD51D,0x34);
	LCD_WriteReg(0xD51E,0x02);
	LCD_WriteReg(0xD51F,0x67);
	LCD_WriteReg(0xD520,0x02);
	LCD_WriteReg(0xD521,0x84);
	LCD_WriteReg(0xD522,0x02);
	LCD_WriteReg(0xD523,0xA4);
	LCD_WriteReg(0xD524,0x02);
	LCD_WriteReg(0xD525,0xB7);
	LCD_WriteReg(0xD526,0x02);
	LCD_WriteReg(0xD527,0xCF);
	LCD_WriteReg(0xD528,0x02);
	LCD_WriteReg(0xD529,0xDE);
	LCD_WriteReg(0xD52A,0x02);
	LCD_WriteReg(0xD52B,0xF2);
	LCD_WriteReg(0xD52C,0x02);
	LCD_WriteReg(0xD52D,0xFE);
	LCD_WriteReg(0xD52E,0x03);
	LCD_WriteReg(0xD52F,0x10);
	LCD_WriteReg(0xD530,0x03);
	LCD_WriteReg(0xD531,0x33);
	LCD_WriteReg(0xD532,0x03);
	LCD_WriteReg(0xD533,0x6D);
	LCD_WriteReg(0xD600,0x00);
	LCD_WriteReg(0xD601,0x33);
	LCD_WriteReg(0xD602,0x00);
	LCD_WriteReg(0xD603,0x34);
	LCD_WriteReg(0xD604,0x00);
	LCD_WriteReg(0xD605,0x3A);
	LCD_WriteReg(0xD606,0x00);
	LCD_WriteReg(0xD607,0x4A);
	LCD_WriteReg(0xD608,0x00);
	LCD_WriteReg(0xD609,0x5C);
	LCD_WriteReg(0xD60A,0x00);
	LCD_WriteReg(0xD60B,0x81);

	LCD_WriteReg(0xD60C,0x00);
	LCD_WriteReg(0xD60D,0xA6);
	LCD_WriteReg(0xD60E,0x00);
	LCD_WriteReg(0xD60F,0xE5);
	LCD_WriteReg(0xD610,0x01);
	LCD_WriteReg(0xD611,0x13);
	LCD_WriteReg(0xD612,0x01);
	LCD_WriteReg(0xD613,0x54);
	LCD_WriteReg(0xD614,0x01);
	LCD_WriteReg(0xD615,0x82);
	LCD_WriteReg(0xD616,0x01);
	LCD_WriteReg(0xD617,0xCA);
	LCD_WriteReg(0xD618,0x02);
	LCD_WriteReg(0xD619,0x00);
	LCD_WriteReg(0xD61A,0x02);
	LCD_WriteReg(0xD61B,0x01);
	LCD_WriteReg(0xD61C,0x02);
	LCD_WriteReg(0xD61D,0x34);
	LCD_WriteReg(0xD61E,0x02);
	LCD_WriteReg(0xD61F,0x67);
	LCD_WriteReg(0xD620,0x02);
	LCD_WriteReg(0xD621,0x84);
	LCD_WriteReg(0xD622,0x02);
	LCD_WriteReg(0xD623,0xA4);
	LCD_WriteReg(0xD624,0x02);
	LCD_WriteReg(0xD625,0xB7);
	LCD_WriteReg(0xD626,0x02);
	LCD_WriteReg(0xD627,0xCF);
	LCD_WriteReg(0xD628,0x02);
	LCD_WriteReg(0xD629,0xDE);
	LCD_WriteReg(0xD62A,0x02);
	LCD_WriteReg(0xD62B,0xF2);
	LCD_WriteReg(0xD62C,0x02);
	LCD_WriteReg(0xD62D,0xFE);
	LCD_WriteReg(0xD62E,0x03);
	LCD_WriteReg(0xD62F,0x10);
	LCD_WriteReg(0xD630,0x03);
	LCD_WriteReg(0xD631,0x33);
	LCD_WriteReg(0xD632,0x03);
	LCD_WriteReg(0xD633,0x6D);
	//LV2 Page 0 enable
	LCD_WriteReg(0xF000,0x55);
	LCD_WriteReg(0xF001,0xAA);
	LCD_WriteReg(0xF002,0x52);
	LCD_WriteReg(0xF003,0x08);
	LCD_WriteReg(0xF004,0x00);
	//Display control
	LCD_WriteReg(0xB100, 0xCC);
	LCD_WriteReg(0xB101, 0x00);
	//Source hold time
	LCD_WriteReg(0xB600,0x05);
	//Gate EQ control
	LCD_WriteReg(0xB700,0x70);
	LCD_WriteReg(0xB701,0x70);
	//Source EQ control (Mode 2)
	LCD_WriteReg(0xB800,0x01);
	LCD_WriteReg(0xB801,0x03);
	LCD_WriteReg(0xB802,0x03);
	LCD_WriteReg(0xB803,0x03);
	//Inversion mode (2-dot)
	LCD_WriteReg(0xBC00,0x02);
	LCD_WriteReg(0xBC01,0x00);
	LCD_WriteReg(0xBC02,0x00);
	//Timing control 4H w/ 4-delay
	LCD_WriteReg(0xC900,0xD0);
	LCD_WriteReg(0xC901,0x02);
	LCD_WriteReg(0xC902,0x50);
	LCD_WriteReg(0xC903,0x50);
	LCD_WriteReg(0xC904,0x50);
	LCD_WriteReg(0x3500,0x00);
	LCD_WriteReg(0x3A00,0x55);  //16-bit/pixel
	LCD_WR_REG(0x1100);
	HAL_Delay(120);
	LCD_WR_REG(0x2900);

	LCD_Display_Dir(0);		
	LCD_BL_ON();				
	LCD_Clear(WHITE);
}

/**
 * @brief  Clears the entire screen to a single color.
 */
void LCD_Clear(uint16_t color)
{
    uint32_t index = 0;
    uint32_t totalpoint = lcddev.width;
    totalpoint *= lcddev.height;

    LCD_SetCursor(0, 0);
    LCD_WriteRAM_Prepare();
    for (index = 0; index < totalpoint; index++)
    {
        LCD->LCD_RAM = color;
    }
}

/**
 * @brief  Fills a rectangular region with a single solid color.
 * @param  sx, sy  Top-left corner
 * @param  ex, ey  Bottom-right corner (inclusive)
 */
void LCD_Fill(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t color)
{          
	uint16_t i, j;
    uint16_t xlen = ex - sx + 1;

    for (i = sy; i <= ey; i++)
    {
        LCD_SetCursor(sx, i);
        LCD_WriteRAM_Prepare();
        for (j = 0; j < xlen; j++) LCD->LCD_RAM = color;
    }
}  

/**
 * @brief  Fills a rectangular region using a per-pixel color buffer
 *         (e.g. for displaying a bitmap/image).
 * @param  color  Pointer to a buffer of (width*height) RGB565 values,
 *                row-major order
 */
void LCD_Color_Fill(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t *color)
{  
	uint16_t height, width;
    uint16_t i, j;
    width = ex - sx + 1;
    height = ey - sy + 1;

    for (i = 0; i < height; i++)
    {
        LCD_SetCursor(sx, sy + i);
        LCD_WriteRAM_Prepare();
        for (j = 0; j < width; j++) LCD->LCD_RAM = color[i * width + j];
    }	  
}  

/**
 * @brief  Draws a line between two points using Bresenham's algorithm.
 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_y; }
    distance = (delta_x > delta_y) ? delta_x : delta_y;

	for (t = 0; t <= distance + 1; t++)
    {
        LCD_DrawPoint(uRow, uCol);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) { xerr -= distance; uRow += incx; }
        if (yerr > distance) { yerr -= distance; uCol += incy; }
    } 
}    

/**
 * @brief  Draws an unfilled rectangle outline.
 */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	LCD_DrawLine(x1, y1, x2, y1);
    LCD_DrawLine(x1, y1, x1, y2);
    LCD_DrawLine(x1, y2, x2, y2);
    LCD_DrawLine(x2, y1, x2, y2);
}

/**
 * @brief  Draws a circle outline using the midpoint (Bresenham) circle algorithm.
 * @param  x0, y0  Center coordinates
 * @param  r       Radius
 */
void LCD_Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r)
{
	int a, b, di;
    a = 0; b = r;
    di = 3 - (r << 1);
    while (a <= b)
    {
        LCD_DrawPoint(x0 + a, y0 - b);
        LCD_DrawPoint(x0 + b, y0 - a);
        LCD_DrawPoint(x0 + b, y0 + a);
        LCD_DrawPoint(x0 + a, y0 + b);
        LCD_DrawPoint(x0 - a, y0 + b);
        LCD_DrawPoint(x0 - b, y0 + a);
        LCD_DrawPoint(x0 - a, y0 - b);
        LCD_DrawPoint(x0 - b, y0 - a);
        a++;
        if (di < 0) di += 4 * a + 6;
        else { di += 10 + 4 * (a - b); b--; }
    }
} 									  

/**
 * @brief  Draws a single ASCII character from the built-in bitmap font.
 * @param  size  Font size: 12, 16, or 24
 * @param  mode  0 = overwrite background with BACK_COLOR,
 *               1 = overlay (transparent background)
 */
void LCD_ShowChar(uint16_t x,uint16_t y,uint8_t num,uint8_t size,uint8_t mode)
{  							  
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    /* Bytes per character glyph, given the font's bitmap dimensions */
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

    num = num - ' ';   /* font data starts at the space character (ASCII 0x20) */

    for (t = 0; t < csize; t++)
    {
        if (size == 12) temp = asc2_1206[num][t];
        else if (size == 16) temp = asc2_1608[num][t];
        else if (size == 24) temp = asc2_2412[num][t];
        else return;

        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80) LCD_Fast_DrawPoint(x, y, POINT_COLOR);
            else if (mode == 0) LCD_Fast_DrawPoint(x, y, BACK_COLOR);
            temp <<= 1;
            y++;
            if (y >= lcddev.height) return;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= lcddev.width) return;
                break;
            }
        }
    }
}   

/**
 * @brief  Integer power function (m^n), used by LCD_ShowNum for digit extraction.
 */
uint32_t LCD_Pow(uint8_t m,uint8_t n)
{
	uint32_t result = 1;
    while (n--) result *= m;
    return result;
}			 

/**
 * @brief  Displays a number with leading zeros suppressed (shown as spaces).
 * @param  len  Number of digit positions to display
 */	 
void LCD_ShowNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t size)
{         	
	uint8_t t, temp;
    uint8_t enshow = 0;
    for (t = 0; t < len; t++)
    {
        temp = (num / LCD_Pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_ShowChar(x + (size / 2) * t, y, ' ', size, 0);
                continue;
            }
            else enshow = 1;
        }
        LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, 0);
    }
} 

/**
 * @brief  Displays a number with configurable leading-zero and overlay behavior.
 * @param  mode  Bit 7: 1 = pad with '0' instead of space
 *               Bit 0: 1 = overlay mode (transparent background)
 */
void LCD_ShowxNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t size,uint8_t mode)
{  
	uint8_t t, temp;
    uint8_t enshow = 0;
    for (t = 0; t < len; t++)
    {
        temp = (num / LCD_Pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                if (mode & 0x80) LCD_ShowChar(x + (size / 2) * t, y, '0', size, mode & 0x01);
                else LCD_ShowChar(x + (size / 2) * t, y, ' ', size, mode & 0x01);
                continue;
            }
            else enshow = 1;
        }
        LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, mode & 0x01);
    }
} 

/**
 * @brief  Displays a string within a bounding box, wrapping to the next
 *         line when the width is exceeded.
 * @param  width, height  Bounding box size; text beyond this is clipped
 */	  
void LCD_ShowString(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t size,const char *p)
{         
	uint8_t x0 = x;
    width += x;
    height += y;
    while ((*p <= '~') && (*p >= ' '))
    {
        if (x >= width) { x = x0; y += size; }
        if (y >= height) break;
        LCD_ShowChar(x, y, *p, size, 0);
        x += size / 2;
        p++;
    }
}
