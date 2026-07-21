  /******************************************************************************
  * @file           : uart_tx.h
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************/
#ifndef __UART_TX_H
#define __UART_TX_H

#include "main.h"
#include "acq_adc.h"

/// @brief Format + transmit one data frame per requirements_spec.md §6.1/§6.2.
void uart_tx_frame(const char *date, const char *time, const SensorData_t *data);

#endif /* __UART_TX_H */