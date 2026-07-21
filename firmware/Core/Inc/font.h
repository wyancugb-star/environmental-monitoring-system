  /******************************************************************************
  * @file           : font.h
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/
#ifndef __FONT_H
#define __FONT_H

#include "main.h"

typedef struct {
    const uint8_t *data;
    uint16_t width;
    uint16_t height;
    uint8_t dataSize;
} tImage;

typedef struct {
    long int code;
    const tImage *image;
} tChar;

typedef struct {
    int length;
    const tChar *chars;
} tFont;

extern const tFont Font_48;
extern const tFont Font_72;

#endif /* __FONT_BIG_H */