/**
  ******************************************************************************
  * @file           : usmart.h
  * @brief          : usmart - UART-based interactive function call debugger
  *
  * @details
  * Allows registered C functions to be invoked directly from a serial
  * terminal (e.g. typing `led_set(1)` over UART calls led_set(1) on
  * the target). Useful for interactive testing without recompiling.
  *
  * Setup:
  *   1. Register functions in usmart_nametab[] (defined in usmart_config.c)
  *   2. Call usmart_dev.init() once at startup
  *   3. Pass received command strings to usmart_execute()
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************
  */

#ifndef __USMART_H
#define __USMART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usmart_str.h"
#include <stdint.h>

/* --- Configuration --- */
#define MAX_FNAME_LEN   30   /* Max function name length; must fit the longest registered name */
#define MAX_PARM        10   /* Max parameters per call (usmart_exe must be updated if changed) */
#define PARM_LEN        200  /* Max combined size of all parameters, in bytes
                                 (UART receive buffer must be at least this size) */

#define USMART_ENTIMX_SCAN  1  /* 1 = use a timer interrupt to periodically call the scan
                                   function; required if runtime statistics are used.
                                   0 = caller must invoke the scan function manually. */
#define USMART_USE_HELP     1  /* 1 = enable help text (~700 bytes); 0 = disable to save space */
#define USMART_USE_WRFUNS   1  /* 1 = enable read/write helper functions for arbitrary
                                   memory-mapped register access */

/* --- Error codes (returned by usmart_get_fname / usmart_get_fparam) --- */
#define USMART_OK           0
#define USMART_FUNCERR      1  /* Malformed function call syntax */
#define USMART_PARMERR      2  /* Invalid parameter value */
#define USMART_PARMOVER     3  /* Too many parameters */
#define USMART_NOFUNCFIND   4  /* No matching function name in usmart_nametab[] */

#define SP_TYPE_DEC  0   /* Display parameter values in decimal */
#define SP_TYPE_HEX  1   /* Display parameter values in hexadecimal */

/**
 * @brief One entry in the registered-function lookup table.
 */
typedef struct
{
    void *func;          /**< Pointer to the target function */
    const char *name;    /**< Function name as typed by the user */
} usmart_func_t;

/**
 * @brief usmart runtime state and the current parsed command's data.
 */
typedef struct
{
    const usmart_func_t *funs;   /**< Pointer to the registered function table */

    void (*init)(uint8_t);              /**< Initializes usmart (arg: system clock in MHz) */
    uint8_t (*cmd_rec)(const char *str); /**< Parses a command string, matching it to a function */
    void (*exe)(void);                  /**< Invokes the matched function with parsed arguments */

    uint8_t fnum;   /**< Number of functions in the table */
    uint8_t pnum;   /**< Number of parameters in the current parsed command */
    uint8_t id;     /**< Index of the matched function in the table */

    uint16_t parmtype;            /**< Bitmask: bit set = string param, clear = numeric param */
    uint8_t  plentbl[MAX_PARM];   /**< Byte length of each parsed parameter */
    char     parm[PARM_LEN];      /**< Packed buffer holding all parsed parameter values */
} usmart_t;

extern const usmart_func_t usmart_nametab[];   /* Defined in usmart_config.c */
extern usmart_t usmart_dev;                     /* Defined in usmart_config.c */

/* USER CODE END Private defines */

/* USER CODE BEGIN Prototypes */

/**
 * @brief  Initializes usmart.
 * @param  sysclk  System/timer clock in MHz, used for runtime statistics timing
 */
void usmart_init(uint8_t sysclk);

/**
 * @brief  Parses a command string and looks up the matching registered function.
 * @retval USMART_OK  or one of the USMART_* error codes
 */
uint8_t usmart_cmd_rec(const char *str);

/**
 * @brief  Invokes the function matched by the most recent usmart_cmd_rec() call.
 */
void usmart_exe(void);

/**
 * @brief  Convenience wrapper: parses and executes a command string in one call.
 *         Typically called directly from the UART receive callback.
 */
void usmart_execute(const char *cmd);

#ifdef __cplusplus
}
#endif

#endif /* __USMART_H */