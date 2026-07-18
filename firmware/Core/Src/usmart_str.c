/**
  ******************************************************************************
  * @file           : usmart_str.c
  * @brief          : functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Yan.
  * All rights reserved.
  *
  ******************************************************************************
  */

#include "usmart_str.h"
#include "usmart.h"		   
#include <sys/_types.h>
#include <stddef.h>

/**
 * @brief  Compares two null-terminated strings for equality.
 * @retval 0  Strings are equal
 * @retval 1  Strings differ
 */
uint8_t usmart_strcmp(const char *str1,const char *str2)
{
	while (1)
    {
        if (*str1 != *str2) return 1;
        if (*str1 == '\0') break;
        str1++;
        str2++;
    }
    return 0;
}

/**
 * @brief  Copies a null-terminated string from src to dst.
 */		   
void usmart_strcopy(const char *src, char *dst)
{
	while (1)
    {
        *dst = *src;
        if (*src == '\0') break;
        src++;
        dst++;
    }
}

/**
 * @brief  Returns the length of a null-terminated string, in bytes.
 */
uint8_t usmart_strlen(const char *str)
{
    uint8_t len = 0;
    while (1)
    {
        if (*str == '\0') break;
        len++;
        str++;
    }
    return len;
}

/**
 * @brief  Integer power function (m^n), used for base conversion in usmart_str2num.
 */
uint32_t usmart_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}	    

/**
 * @brief  Converts a numeric string to a uint32_t value.
 *
 * @note   Supports decimal, and hexadecimal in "0X..." format (hex
 *         letters must be uppercase A-F). Negative numbers are not
 *         supported.
 *
 * @param  res  Output: the converted numeric value
 * @retval 0  Success
 * @retval 1  Invalid character (not decimal/hex digit)
 * @retval 2  Hex string too short (less than 3 chars, i.e. no digits after "0X")
 * @retval 3  Hex string missing the required "0X" prefix
 * @retval 4  Empty numeric string
 */
uint8_t usmart_str2num(const char *str, uint32_t *res)
{
    uint32_t t;
    uint8_t bnum = 0;
    const char *p;
    uint8_t hexdec = 10;   /* assume decimal unless a hex marker is found */
    p = str;
    *res = 0;

    /* First pass: validate characters and detect hex vs decimal */
    while (1)
    {
        if ((*p <= '9' && *p >= '0') || (*p <= 'F' && *p >= 'A') || (*p == 'X' && bnum == 1))
        {
            if (*p >= 'A') hexdec = 16;   /* letter present -> hex */
            bnum++;
        }
        else if (*p == '\0') break;
        else return 1;
        p++;
    }

    p = str;
    if (hexdec == 16)
    {
        if (bnum < 3) return 2;                        /* "0X" alone is invalid */
        if (*p == '0' && (*(p + 1) == 'X'))
        {
            p += 2;      /* skip the "0X" prefix */
            bnum -= 2;
        }
        else return 3;
    }
    else if (bnum == 0) return 4;

    /* Second pass: accumulate the value digit by digit (most significant first) */
    while (1)
    {
        if (bnum) bnum--;
        if (*p <= '9' && *p >= '0') t = *p - '0';
        else t = *p - 'A' + 10;
        *res += t * usmart_pow(hexdec, bnum);
        p++;
        if (*p == '\0') break;
    }
    return 0;
}

/**
 * @brief  Extracts the command name (the substring up to the first
 *         space or end of string) from a raw input line.
 * @param  maxlen  Size limit for cmdname, to guard against overflow
 * @retval 0  Success
 * @retval 1  Command name exceeds maxlen
 */
uint8_t usmart_get_cmdname(const char *str, char *cmdname, uint8_t *nlen, uint8_t maxlen)
{
    *nlen = 0;
    while (*str != ' ' && *str != '\0')
    {
        *cmdname = *str;
        str++;
        cmdname++;
        (*nlen)++;
        if (*nlen >= maxlen) return 1;
    }
    *cmdname = '\0';
    return 0;
}

/**
 * @brief  Skips leading spaces and returns a pointer to the next
 *         non-space character.
 */
const char *usmart_search_nextc(const char *str)
{
    while (*str == ' ')
    {
        str++;
    }
    return str;
}

/**
 * @brief  Parses a C-style function signature/name out of a command
 *         string, e.g. "void led_set(u8)" -> fname="led_set", pnum=1, rval=0.
 *
 * @details
 * This is the most involved parser in the module. It runs in two passes:
 *   1. Scans up to the first '(' to detect a leading return-type keyword
 *      (only "void" is checked for; anything else implies a return value).
 *   2. Walks the string tracking parenthesis depth (`fover`) and whether
 *      the cursor is inside a quoted string (`nchar`), to correctly find
 *      the function name boundary and count comma-separated arguments
 *      without being confused by commas inside string literals.
 *
 * A parameter list of exactly "void" is treated as zero arguments.
 *
 * @param  fname  Output: extracted function name
 * @param  pnum   Output: number of parameters
 * @param  rval   Output: 0 = function returns void, 1 = has a return value
 * @retval 0             Success
 * @retval USMART_FUNCERR  Malformed function call (e.g. unbalanced parentheses)
 */
uint8_t usmart_get_fname(const char *str, char *fname, uint8_t *pnum, uint8_t *rval)
{
    uint8_t res;
    uint8_t fover = 0;        /* parenthesis nesting depth */
    const char *strtemp;
    uint8_t offset = 0;
    uint8_t parmnum = 0;
    uint8_t temp = 1;
    char fpname[6];           /* holds "void" or the first param's leading chars */
    uint8_t fplcnt = 0;
    uint8_t pcnt = 0;
    uint8_t nchar;

    /* --- Pass 1: detect return type keyword before the function name --- */
    strtemp = str;
    while (*strtemp != '\0')
    {
        if (*strtemp != ' ' && (pcnt & 0x7F) < 5)
        {
            if (pcnt == 0) pcnt |= 0x80;   /* high bit: "started capturing" flag */
            if (((pcnt & 0x7f) == 4) && (*strtemp != '*')) break;
            fpname[pcnt & 0x7f] = *strtemp;
            pcnt++;
        }
        else if (pcnt == 0X85) break;
        strtemp++;
    }
    if (pcnt)
    {
        fpname[pcnt & 0x7f] = '\0';
        if (usmart_strcmp(fpname, "void") == 0) *rval = 0;
        else *rval = 1;
        pcnt = 0;
    }

    /* --- Locate the actual start of the function name (skip return type / '*') --- */
    res = 0;
    strtemp = str;
    while (*strtemp != '(' && *strtemp != '\0')
    {
        strtemp++;
        res++;
        if (*strtemp == ' ' || *strtemp == '*')
        {
            nchar = *usmart_search_nextc(strtemp);
            if (nchar != '(' && nchar != '*') offset = res;
        }
    }
    strtemp = str;
    if (offset) strtemp += offset + 1;

    /* --- Pass 2: extract the function name and count arguments --- */
    res = 0;
    nchar = 0;   /* inside a quoted string? 0=no, 1=yes */
    while (1)
    {
        if (*strtemp == 0)
        {
            res = USMART_FUNCERR;
            break;
        }
        else if (*strtemp == '(' && nchar == 0) fover++;
        else if (*strtemp == ')' && nchar == 0)
        {
            if (fover) fover--;
            else res = USMART_FUNCERR;   /* ')' without a matching '(' */
            if (fover == 0) break;
        }
        else if (*strtemp == '"') nchar = !nchar;   /* toggle in/out of a quoted string */

        if (fover == 0)
        {
            /* still inside the function name (before '(') */
            if (*strtemp != ' ')
            {
                *fname = *strtemp;
                fname++;
            }
        }
        else
        {
            /* inside the argument list: count commas as argument separators */
            if (*strtemp == ',')
            {
                temp = 1;
                pcnt++;
            }
            else if (*strtemp != ' ' && *strtemp != '(')
            {
                if (pcnt == 0 && fplcnt < 5)
                {
                    /* capture the first argument's leading chars to check for "void" */
                    fpname[fplcnt] = *strtemp;
                    fplcnt++;
                }
                temp++;
            }
            if (fover == 1 && temp == 2)
            {
                temp++;      /* guard against double-counting the same argument */
                parmnum++;
            }
        }
        strtemp++;
    }

    if (parmnum == 1)
    {
        fpname[fplcnt] = '\0';
        if (usmart_strcmp(fpname, "void") == 0) parmnum = 0;   /* "(void)" = no arguments */
    }
    *pnum = parmnum;
    *fname = '\0';
    return res;
}

/**
 * @brief  Extracts a single argument from a comma-separated parameter
 *         list, determining whether it's numeric or a quoted string.
 *
 * @note   Numeric characters are upper-cased in place (e.g. "0xff" ->
 *         "0XFF") so usmart_str2num's uppercase-hex requirement is met.
 *         Backslash-escaped characters inside strings are copied
 *         literally (the escape character itself is dropped).
 *
 * @param  fparm  Output buffer for the extracted argument text
 * @param  ptype  Output: 0 = numeric, 1 = string, 0xFF = malformed
 * @return Number of characters consumed from str
 */
uint8_t usmart_get_aparm(const char *str, char *fparm, uint8_t *ptype)
{
    uint8_t i = 0;
    uint8_t enout = 0;    /* found the separator for the *next* argument */
    uint8_t type = 0;
    uint8_t string = 0;   /* currently inside a quoted string? */

    while (1)
    {
        if (*str == ',' && string == 0) enout = 1;
        if ((*str == ')' || *str == '\0') && string == 0) break;

        if (type == 0)
        {
            if ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f') ||
                (*str >= 'A' && *str <= 'F') || *str == 'X' || *str == 'x')
            {
                if (enout) break;
                if (*str >= 'a') *fparm = *str - 0x20;   /* lowercase -> uppercase */
                else *fparm = *str;
                fparm++;
            }
            else if (*str == '"')
            {
                if (enout) break;
                type = 1;
                string = 1;
            }
            else if (*str != ' ' && *str != ',')
            {
                type = 0xFF;
                break;
            }
        }
        else
        {
            if (*str == '"') string = 0;
            if (enout) break;
            if (string)
            {
                if (*str == '\\')   /* escape character: skip it, copy the next char verbatim */
                {
                    str++;
                    i++;
                }
                *fparm = *str;
                fparm++;
            }
        }
        i++;
        str++;
    }
    *fparm = '\0';
    *ptype = type;
    return i;
}

/**
 * @brief  Returns the starting offset of the num-th parameter within
 *         the packed parameter buffer (usmart_dev.parm), based on the
 *         lengths recorded in usmart_dev.plentbl.
 */
uint8_t usmart_get_parmpos(uint8_t num)
{
    uint8_t temp = 0;
    uint8_t i;
    for (i = 0; i < num; i++) temp += usmart_dev.plentbl[i];
    return temp;
}

/**
 * @brief  Parses all arguments from a command string and stores them
 *         into usmart_dev.parm, ready to be passed to the target function.
 *
 * @details
 * Numeric arguments are stored as 4-byte uint32_t values; string
 * arguments are copied as null-terminated text. usmart_dev.parmtype
 * tracks which slots are strings (bit set) vs numbers (bit clear), and
 * usmart_dev.plentbl records each argument's byte length so
 * usmart_get_parmpos can compute offsets for variable-length arguments.
 *
 * @param  parn  Output: number of parameters found (0 = void)
 * @retval USMART_OK        Success
 * @retval USMART_FUNCERR   No '(' found in the string
 * @retval USMART_PARMERR   A numeric argument failed to parse
 * @retval USMART_PARMOVER  More arguments than MAX_PARM
 */
uint8_t usmart_get_fparam(const char *str, uint8_t *parn)
{
    uint8_t i, type;
    uint32_t res;
    uint8_t n = 0;
    uint8_t len;
    char tstr[PARM_LEN + 1];

    for (i = 0; i < MAX_PARM; i++) usmart_dev.plentbl[i] = 0;

    while (*str != '(')
    {
        str++;
        if (*str == '\0') return USMART_FUNCERR;
    }
    str++;   /* move past '(' */

    while (1)
    {
        i = usmart_get_aparm(str, tstr, &type);
        str += i;
        switch (type)
        {
            case 0:   /* numeric */
                if (tstr[0] != '\0')
                {
                    i = usmart_str2num(tstr, &res);
                    if (i) return USMART_PARMERR;
                    *(uint32_t *)(usmart_dev.parm + usmart_get_parmpos(n)) = res;
                    usmart_dev.parmtype &= ~(1 << n);
                    usmart_dev.plentbl[n] = 4;
                    n++;
                    if (n > MAX_PARM) return USMART_PARMOVER;
                }
                break;
            case 1:   /* string */
                len = usmart_strlen(tstr) + 1;   /* include null terminator */
                usmart_strcopy(tstr, &usmart_dev.parm[usmart_get_parmpos(n)]);
                usmart_dev.parmtype |= 1 << n;
                usmart_dev.plentbl[n] = len;
                n++;
                if (n > MAX_PARM) return USMART_PARMOVER;
                break;
            case 0xFF:   /* malformed */
                return USMART_PARMERR;
        }
        if (*str == ')' || *str == '\0') break;
    }
    *parn = n;
    return USMART_OK;
}


