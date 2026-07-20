  /******************************************************************************
  * @file           : rtc_mgr.h
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/
#ifndef __RTC_MGR_H
#define __RTC_MGR_H

#include "main.h"
#include "rtc.h"

/// @brief Init RTC; if backup-domain flag is unset, write sentinel 2000-01-01 00:00:00 (REQ-SYS-003).
void rtc_mgr_init(void);

/// @brief Get current RTC time as separate date/time strings for display/UART.
void rtc_mgr_get(char *date_out, char *time_out); // date_out: "YYYY-MM-DD", time_out: "HH:MM:SS", both caller-allocated, min 11 and 9 bytes respectively

/**
 * @brief  Set RTC time. Registered with usmart so it's callable over UART (REQ-COMM-003).
 * @note   Prints "RTC SET OK: <date> <time>" over UART on success (§6.3).
 */
void set_rtc_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);

#endif /* __RTC_MGR_H */