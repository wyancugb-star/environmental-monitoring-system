/**
 * @file    acq_adc.c
 * @brief   Implementation of light + internal-temperature acquisition.
 * @details Temp (ADC1) and light (ADC3) are on separate ADC peripherals --
 *          STM32F4's internal temp channel is only routable on ADC1 -- so
 *          each is read independently, no shared-channel switching needed.
 *          Internal temp conversion constants (V25, Avg_Slope) are
 *          datasheet-typical, not per-unit calibrated -- see
 *          requirements_spec.md §7.2 for the accuracy caveat.
 */
#include "acq_adc.h"
#include "app_config.h"
#include "adc.h"

/* --- shared ADC sampling helper -----------------------------------------*/

/**
 * @brief  Sample a given ADC channel `times` times and return the average.
 *         Shared by both the temp (ADC1) and light (ADC3) paths, and by
 *         their respective raw-value debug print helpers, so the
 *         Start/PollForConversion/GetValue/Stop/Delay sequence exists
 *         in exactly one place.
 * @param  hadc   Pointer to the ADC handle (e.g. &hadc1, &hadc3).
 * @param  times  Number of samples to average.
 * @retval Raw ADC average (12-bit range, 0-4095).
 */
static uint16_t adc_read_average(ADC_HandleTypeDef *hadc, uint8_t times)
{
    uint32_t sum = 0;
    for (uint8_t t = 0; t < times; t++) {
        HAL_ADC_Start(hadc);
        HAL_ADC_PollForConversion(hadc, 10);
        sum += HAL_ADC_GetValue(hadc);
        HAL_ADC_Stop(hadc);
        HAL_Delay(5);
    }
    return (uint16_t)(sum / times);
}

/* --- internal temperature (ADC1) --------------------------------------*/

/**
 * @brief  Convert internal-temp ADC average to °C.
 * @param  adc_avg  Raw ADC average from adc_read_average(&hadc1, ...).
 * @retval Temperature in °C. Clamping to [TEMP_MIN_C, TEMP_MAX_C] is the
 *         caller's responsibility (see acq_read()), not done here.
 * @note   Formula per requirements_spec.md §9. 4095, not 4096 -- a 12-bit
 *         ADC's max code is 4095 (4096 distinct codes, range 0..4095).
 */
static float adc_to_temp_c(uint16_t adc_avg)
{
    float v_sense = (float)adc_avg * (3.3f / 4095.0f);
    return ((v_sense - TEMP_V25) / TEMP_AVG_SLOPE) + 25.0f;
}

/* --- light sensor (ADC3) -----------------------------------------------*/

/**
 * @brief  Convert a raw light-sensor ADC reading to a 0-100% intensity value.
 * @param  adcValue  Raw ADC average from adc_read_average(&hadc3, ...).
 * @param[out] clamped  Set to 1 if adcValue fell outside the calibrated
 *                       [LIGHT_ADC_BRIGHT, LIGHT_ADC_DARK] range and was
 *                       clamped to a boundary (REQ-SYS-002), else set to 0.
 * @retval Light intensity 0-100 (0 = darkest, 100 = brightest)
 * @note   Sensor wiring means higher ADC reading = darker; adjust the
 *         comparison direction here if your circuit is wired the other way.
 */
static uint8_t adc_to_light_pct(uint16_t adcValue, uint8_t *clamped)
{
    *clamped = 0;

    if (adcValue >= LIGHT_ADC_DARK) {
        *clamped = (adcValue > LIGHT_ADC_DARK); // exact boundary value doesn't count as clamped
        return 0;
    }
    if (adcValue <= LIGHT_ADC_BRIGHT) {
        *clamped = (adcValue < LIGHT_ADC_BRIGHT);
        return 100;
    }

    // Linear interpolation between the two calibrated endpoints
    return (uint8_t)(100UL * (LIGHT_ADC_DARK - adcValue) / (LIGHT_ADC_DARK - LIGHT_ADC_BRIGHT));
}

/* --- public API --------------------------------------------------------*/

/**
 * @brief  One-time acquisition module init.
 * @note   ADC1 (temp) and ADC3 (light) are both brought up by CubeMX-generated
 *         MX_ADC1_Init()/MX_ADC3_Init(). Nothing extra needed yet -- reserved
 *         for future setup steps.
 */
void acq_adc_init(void)
{
    // Nothing needed yet.
}

/**
 * @brief  Read + convert + clamp both light and internal-temp channels.
 * @retval Populated SensorData_t with clamp/_err flags applied per REQ-SYS-002.
 */
SensorData_t acq_read(void)
{
    SensorData_t data = {0};

    uint16_t temp_adc = adc_read_average(&hadc1, 10);
    float temp_c = adc_to_temp_c(temp_adc);
    if (temp_c < TEMP_MIN_C) { temp_c = TEMP_MIN_C; data.temp_err = 1; }
    else if (temp_c > TEMP_MAX_C) { temp_c = TEMP_MAX_C; data.temp_err = 1; }
    data.temp_c = temp_c;

    uint16_t light_adc = adc_read_average(&hadc3, LSENS_READ_TIMES);
    uint8_t light_clamped = 0;
    data.light_pct = adc_to_light_pct(light_adc, &light_clamped);
    data.light_err = light_clamped;

    return data;
}

/* --- debug helpers (usmart-registered) ----------------------------------*/

/**
 * @brief  Debug helper: print raw light-sensor ADC average, no conversion.
 *         Register with usmart temporarily for calibration; can stay
 *         registered permanently too, doesn't hurt.
 */
/**
 * @brief  Debug helper: print raw light-sensor ADC average, no conversion.
 *         Register with usmart temporarily for calibration; can stay
 *         registered permanently too, doesn't hurt.
 */
void light_adc_raw_print(void)
{
    uint16_t adc_avg = adc_read_average(&hadc3, LSENS_READ_TIMES);
    printf("Light raw ADC avg: %u\r\n", adc_avg);
}

/**
 * @brief  Debug helper: print raw internal-temp ADC average and the
 *         converted °C value, for verifying/calibrating against a
 *         reference thermometer.
 */
void temp_adc_raw_print(void)
{
    uint16_t adc_avg = adc_read_average(&hadc1, 10);
    float temp_c = adc_to_temp_c(adc_avg);
    printf("Temp raw ADC avg: %u, converted: %.1f C\r\n", adc_avg, temp_c);
}