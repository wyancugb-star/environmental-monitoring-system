/**
 * @file    uart_tx.c
 * @brief   Formats and transmits the 1Hz UART data frame.
 * @details Frame format per requirements_spec.md §6.1:
 *              DATE:YYYY-MM-DD,TIME:HH:MM:SS,TEMP:##.#,LIGHT:###\n
 *          with an "_ERR" suffix appended to a field's value when that
 *          field's clamp flag is set (§6.2), e.g. "TEMP:85.0_ERR".
 *          Uses blocking HAL_UART_Transmit() -- a ~50 byte frame at
 *          115200 baud takes well under 1ms, negligible against the 1s
 *          scheduling budget (design.md §7), and keeps this consistent
 *          with the existing printf/usmart UART path, which is also
 *          blocking.
 */
#include "uart_tx.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

void uart_tx_frame(const char *date, const char *time, const SensorData_t *data)
{
    char frame[64];

    int len = snprintf(frame, sizeof(frame),
                        "DATE:%s,TIME:%s,TEMP:%.1f%s,LIGHT:%d%s\r\n",
                        date, time,
                        data->temp_c,  data->temp_err  ? "_ERR" : "",
                        data->light_pct, data->light_err ? "_ERR" : "");

    if (len > 0) {
        HAL_UART_Transmit(&huart1, (uint8_t *)frame, (uint16_t)len, HAL_MAX_DELAY);
    }
}