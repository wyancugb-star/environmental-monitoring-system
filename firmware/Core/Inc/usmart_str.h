/**
  ******************************************************************************
  * @file           : usmart_str.h
  * @brief          : usmart command-line string parsing utilities
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************
  */

#ifndef __USMART_STR_H
#define __USMART_STR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t usmart_get_parmpos(uint8_t num);
uint8_t usmart_strcmp(const char *str1, const char *str2);
uint32_t usmart_pow(uint8_t m, uint8_t n);
uint8_t usmart_str2num(const char *str, uint32_t *res);
uint8_t usmart_get_cmdname(const char *str, char *cmdname, uint8_t *nlen, uint8_t maxlen);
uint8_t usmart_get_fname(const char *str, char *fname, uint8_t *pnum, uint8_t *rval);
uint8_t usmart_get_aparm(const char *str, char *fparm, uint8_t *ptype);
uint8_t usmart_get_fparam(const char *str, uint8_t *parn);

#ifdef __cplusplus
}
#endif

#endif /* __USMART_STR_H */