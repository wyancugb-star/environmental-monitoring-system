/**
  ******************************************************************************
  * @file           : usmart.c
  * @brief          : usmart command parsing and function invocation core
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************
  */
#include "usmart.h"
#include "usart.h"

/* Built-in system commands (not user-registered functions) */
const char *sys_cmd_tab[] =
{
    "?",
    "help",
    "list",
};

/**
 * @brief  Handles the built-in system commands (help, list), as opposed
 *         to user-registered functions.
 * @retval 0  Command recognized and handled
 * @retval USMART_FUNCERR  Not a recognized system command
 */
uint8_t usmart_sys_cmd_exe(char *str)
{
	uint8_t i;
    char sfname[MAX_FNAME_LEN];
    usmart_get_cmdname(str, sfname, &i, MAX_FNAME_LEN);
    str += i;

	for (i = 0; i < sizeof(sys_cmd_tab) / sizeof(sys_cmd_tab[0]); i++)
    {
        if (usmart_strcmp(sfname, sys_cmd_tab[i]) == 0) break;
    }

	switch(i)
	{					   
        case 0:
        case 1:   /* "?" or "help" */
            printf("\r\n");
#if USMART_USE_HELP
			printf("?:      Get help information\r\n");
			printf("help:   Get help information\r\n");
			printf("list:   List of functions\r\n\n");
			printf("Please enter the function name and parameters according to the required programming format, and press the Enter key to finish..\r\n");    
#else
			printf("Command disabled\r\n");
#endif
			break;
		case 2:   /* "list": print all registered function names */
            printf("\r\n-------------------------Function List--------------------------- \r\n");
            for (i = 0; i < usmart_dev.fnum; i++) printf("%s\r\n", usmart_dev.funs[i].name);
            printf("\r\n");
            break;

        default:
            return USMART_FUNCERR;
	}
	return 0;
}

/**
 * @brief  usmart initialization entry point.
 * @note   Currently unused (no runtime setup required with
 *         USMART_ENTIMX_SCAN disabled). Kept as a stub to satisfy
 *         the usmart_dev.init function pointer contract; reserved
 *         for future timer-based runtime statistics setup.
 */
void usmart_init(uint8_t sysclk)
{
}

/**
 * @brief  Parses a received command string and matches it against the
 *         registered function table.
 *
 * @details
 * Parses the incoming string's function name/parameter count, then
 * linearly searches usmart_dev.funs[] for a name match. If found,
 * also parses the actual argument values into usmart_dev.parm via
 * usmart_get_fparam().
 *
 * @retval USMART_OK            Match found, arguments parsed
 * @retval USMART_PARMERR       Fewer arguments supplied than the function expects
 * @retval USMART_NOFUNCFIND    No registered function matches the name
 * @retval (other usmart_get_fname/usmart_get_fparam error codes)
 */
uint8_t usmart_cmd_rec(const char*str) 
{
	uint8_t sta, i, rval;
    uint8_t rpnum, spnum;
    char rfname[MAX_FNAME_LEN];
    char sfname[MAX_FNAME_LEN];

	sta = usmart_get_fname(str, rfname, &rpnum, &rval);
    if (sta) return sta;

	for (i = 0; i < usmart_dev.fnum; i++)
    {
        sta = usmart_get_fname(usmart_dev.funs[i].name, sfname, &spnum, &rval);
        if (sta) return sta;
        if (usmart_strcmp(sfname, rfname) == 0)
        {
            if (spnum > rpnum) return USMART_PARMERR;
            usmart_dev.id = i;
            break;
        }
    }
    sta = usmart_get_fparam(str, &i);
    if (sta) return sta;
    usmart_dev.pnum = i;
    return USMART_OK;
}

/**
 * @brief  Invokes the function matched by the most recent usmart_cmd_rec()
 *         call, passing the parsed arguments, and prints the call and
 *         its result to UART in the form "func(arg1,arg2,...)=0xRESULT".
 *
 * @note   Calls through an untyped function pointer
 *         `(uint32_t(*)())`, meaning the compiler does NOT verify
 *         that argument types/count match the target function's real
 *         signature. This is necessary to support calling arbitrary
 *         registered functions generically, but relies on
 *         usmart_nametab[] being registered correctly - a mismatch
 *         here is undefined behavior, not a compile-time error.
 *
 *         Supports up to MAX_PARM (10) arguments; each case below
 *         is functionally identical, just with a different argument
 *         count in the call expression.
 */
void usmart_exe(void)
{
	uint8_t id, i;
    uint32_t res = 0;
    uint32_t temp[MAX_PARM];
    char sfname[MAX_FNAME_LEN];
    uint8_t pnum, rval;

	id = usmart_dev.id;
    if (id >= usmart_dev.fnum) return;

	usmart_get_fname(usmart_dev.funs[id].name, sfname, &pnum, &rval);
    printf("\r\n%s(", sfname);

	for (i = 0; i < pnum; i++)
    {
        if (usmart_dev.parmtype & (1 << i))
        {
            /* String argument: print quoted, pass a pointer to the stored text */
            printf("\"%s\"", usmart_dev.parm + usmart_get_parmpos(i));
            temp[i] = (uint32_t)&(usmart_dev.parm[usmart_get_parmpos(i)]);
        }
        else
        {
            /* Numeric argument */
            temp[i] = *(uint32_t *)(usmart_dev.parm + usmart_get_parmpos(i));
            printf("0x%08lX", (unsigned long)temp[i]);
        }
        if (i != pnum - 1) printf(",");
    }

	printf(")");

    switch (usmart_dev.pnum)
    {
        case 0: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(); break;
        case 1: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0]); break;
        case 2: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1]); break;
        case 3: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2]); break;
        case 4: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3]); break;
        case 5: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4]); break;
        case 6: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],temp[5]); break;
        case 7: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],temp[5],temp[6]); break;
        case 8: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],temp[5],temp[6],temp[7]); break;
        case 9: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],temp[5],temp[6],temp[7],temp[8]); break;
        case 10: res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],temp[5],temp[6],temp[7],temp[8],temp[9]); break;
    }

	if (rval == 1) printf("0x%08lX", (unsigned long)res);
    else printf(";\r\n");
}

/**
 * @brief  Top-level entry point: parses and executes a raw command
 *         string received over UART. Falls back to system commands
 *         (help/list) if no registered function matches.
 * @note   Typically called directly from the UART receive callback.
 */
void usmart_execute(const char *cmd)
{
    uint8_t sta;
    uint8_t res;

    sta = usmart_dev.cmd_rec((char *)cmd);

    if (sta == 0)
    {
        usmart_dev.exe();
        return;
    }

    /* Not a registered function - try built-in system commands */
    res = usmart_sys_cmd_exe((char *)cmd);
    if (res != USMART_FUNCERR)
    {
        sta = res;
    }

    switch (sta)
    {
        case 0:
            break;
        case USMART_FUNCERR:
            printf("Invalid function!\r\n");
            break;
        case USMART_PARMERR:
            printf("Parameter error!\r\n");
            break;
        case USMART_PARMOVER:
            printf("Too many parameters!\r\n");
            break;
        case USMART_NOFUNCFIND:
            printf("No matching function found!\r\n");
            break;
        default:
            break;
    }
}
