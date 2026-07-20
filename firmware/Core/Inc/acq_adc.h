/**
 * @file    acq_adc.h
 * @brief   Light (ADC3) + internal-temperature (ADC1) acquisition module.
 * @details Single-file acquisition module -- light sensor logic (previously
 *          lsens.c/.h) is folded in directly since both readings serve the
 *          same "acquisition" responsibility and don't need separate files.
 *          Usage: call acq_adc_init() once after MX_ADC1_Init()/MX_ADC3_Init(),
 *          then call acq_read() once per second from the main loop scheduler.
 */
#ifndef __ACQ_ADC_H
#define __ACQ_ADC_H

#include "main.h"
#include "adc.h"

typedef struct {
    float   temp_c;      ///< Temperature in °C, 1 decimal precision
    uint8_t temp_err;     ///< 1 if temp was clamped (REQ-SYS-002), else 0
    uint8_t light_pct;    ///< Light intensity 0-100%
    uint8_t light_err;    ///< 1 if light was clamped (REQ-SYS-002), else 0
} SensorData_t;

void acq_adc_init(void);
void light_adc_raw_print(void);
void temp_adc_raw_print(void);

SensorData_t acq_read(void);


#endif /* __ACQ_ADC_H */