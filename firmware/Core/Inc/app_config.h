#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* Internal temperature sensor conversion (requirements_spec.md §9/§7.2) */
#define TEMP_V25          0.76f
#define TEMP_AVG_SLOPE    0.0025f
#define TEMP_MIN_C        -40.0f
#define TEMP_MAX_C         85.0f

/* Light sensor (formerly lsens.h) */
#define LSENS_READ_TIMES        10
#define LIGHT_ADC_DARK          2800    // adjust once you've measured real full-dark endpoints
#define LIGHT_ADC_BRIGHT        110     // adjust once you've measured real full-bright endpoints
#define LSENS_MAX_LIGHT_VALUE   100

#endif /* __APP_CONFIG_H */