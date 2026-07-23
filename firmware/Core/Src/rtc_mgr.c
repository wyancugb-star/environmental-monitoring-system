/******************************************************************************
  * @file           : rtc_mgr.c
  * @brief          : RTC management: sentinel-default init, get/set time.
  * @details        : On first boot (or after VBAT loss), the RTC is left at
  *                    a sentinel default (2000-01-01 00:00:00) rather than a
  *                    hardcoded build-time value, so an unset clock is
  *                    visibly distinguishable from a real timestamp
  *                    (REQ-SYS-003). The sentinel is not persisted via the
  *                    backup-domain flag -- only a real set_rtc_time() call
  *                    (REQ-COMM-003) marks the RTC as "actually set"
  *                    (REQ-SYS-004).
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/

#include "rtc_mgr.h"

#define RTC_BKP_SET_FLAG   0xA5A5U   // marker written to RTC_BKP_DR1 once set_rtc_time() succeeds

#define SET_TIME_PREFIX "SET_TIME:"

/**
 * @brief  Write time+date to the RTC hardware, no side effects.
 * @details Shared by rtc_mgr_init() (sentinel default) and set_rtc_time()
 *          (user-issued value), so the SetTime/SetDate sequence exists in
 *          exactly one place.
 * @note   year is the full year, e.g. 2026, not a 2-digit value.
 */
static void rtc_write_time_date(uint16_t year, uint8_t month, uint8_t day,
                                 uint8_t hour, uint8_t min, uint8_t sec)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    sTime.Hours   = hour;
    sTime.Minutes = min;
    sTime.Seconds = sec;
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    sDate.Year    = (uint8_t)(year - 2000);
    sDate.Month   = month;
    sDate.Date    = day;
    sDate.WeekDay = RTC_WEEKDAY_MONDAY; // placeholder, not used by requirements_spec.md §6.1 format
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

/**
 * @brief  Init RTC. If the backup-domain flag isn't set (true first boot,
 *         or VBAT was lost -- REQ-SYS-004), leaves the RTC at the sentinel
 *         default 2000-01-01 00:00:00 (REQ-SYS-003) instead of a hardcoded
 *         "current" date, so an unset clock is obvious in logs.
 * @note   Does NOT write the backup flag here -- only set_rtc_time() does,
 *         since that's what actually marks the RTC as intentionally set.
 */
void rtc_mgr_init(void)
{
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != RTC_BKP_SET_FLAG) {
        rtc_write_time_date(2026, 7, 20, 12, 0, 0); // sentinel default, see REQ-SYS-003
    }
    // else: RTC already holds a previously-set time (REQ-SYS-004), leave it alone
}

/**
 * @brief  Set RTC time. Registered with usmart so it's callable over UART
 *         (REQ-COMM-003). Marks the backup-domain flag so future boots
 *         know the RTC was intentionally set (REQ-SYS-004), and echoes a
 *         confirmation line over UART (requirements_spec.md §6.3).
 */
void set_rtc_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
    rtc_write_time_date(year, month, day, hour, min, sec);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_SET_FLAG);

    printf("RTC SET OK: %04d-%02d-%02d %02d:%02d:%02d\r\n", year, month, day, hour, min, sec);
}

/**
 * @brief  Read current RTC date/time as separate strings for display/UART.
 * @param[out] date_out  Caller-allocated buffer, min 11 bytes ("YYYY-MM-DD\0")
 * @param[out] time_out  Caller-allocated buffer, min 9 bytes ("HH:MM:SS\0")
 * @note   HAL_RTC_GetDate() must be called immediately after HAL_RTC_GetTime(),
 *         with nothing in between -- this is required by the RTC's shadow
 *         register mechanism (HAL_RTC_GetTime() is what latches both Time
 *         and Date together; calling GetDate out of order or after other
 *         code runs can read a Date that doesn't match the latched Time).
 */
void rtc_mgr_get(char *date_out, char *time_out)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); // must immediately follow GetTime -- see note above

    sprintf(date_out, "%04d-%02d-%02d", sDate.Year + 2000, sDate.Month, sDate.Date);
    sprintf(time_out, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
}

/**
 * @brief  Parses "SET_TIME:YYYY-MM-DD,HH:MM:SS" and calls set_rtc_time().
 * @details This is the GUI-facing command channel reserved in design.md §3.5,
 *          coexisting with usmart on the same UART (requirements_spec.md §6.4).
 *          It reuses set_rtc_time() directly -- same backup-flag persistence
 *          (REQ-SYS-004) and same "RTC SET OK: ..." echo as the usmart path,
 *          so there's exactly one place that actually writes the RTC.
 */
uint8_t try_handle_set_time_command(const char *cmd)
{
    size_t prefix_len = strlen(SET_TIME_PREFIX);

    if (strncmp(cmd, SET_TIME_PREFIX, prefix_len) != 0) {
        return 0; // not a SET_TIME: command -- let usmart_execute() handle it
    }

    int year, month, day, hour, min, sec;
    int fields = sscanf(cmd + prefix_len, "%d-%d-%d,%d:%d:%d",
                         &year, &month, &day, &hour, &min, &sec);

    if (fields != 6) {
        printf("SET_TIME parse error: expected SET_TIME:YYYY-MM-DD,HH:MM:SS\r\n");
        return 1; // it WAS a SET_TIME: command, just malformed -- still don't fall through to usmart
    }

    set_rtc_time((uint16_t)year, (uint8_t)month, (uint8_t)day,
                 (uint8_t)hour, (uint8_t)min, (uint8_t)sec);
    return 1;
}