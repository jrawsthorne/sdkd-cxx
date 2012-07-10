/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2011 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

/**
 * Debugging routines for cbsdkd. Mostly adapted from the
 * author's 'yolog' project.
 *
 * @author M. Nunberg
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "debug.h"

#define _FG "3"
#define _BG "4"
#define _BRIGHT_FG "1"
#define _INTENSE_FG "9"
#define _DIM_FG "2"
#define _YELLOW "3"
#define _WHITE "7"
#define _MAGENTA "5"
#define _CYAN "6"
#define _BLUE "4"
#define _GREEN "2"
#define _RED "1"
#define _BLACK "0"
/*Logging subsystem*/

static const char *Color_title_fmt = "\033["  _INTENSE_FG _MAGENTA "m";
static const char *Color_reset_fmt = "\033[0m";

cbsdkd_loglevel_t cbsdkd_Default_Log_Level = CBSDKD_LOGLVL_WARN;

static void init_logging(cbsdkd_debug_st *debugp)
{
    char *tmp_env;
    int max_level;

    debugp->initialized = 1;

    if (!debugp->out) {
        debugp->out = stderr;
    }

    if ((tmp_env = getenv(CBSDKD_DEBUG_ENV_ENABLE)) != NULL) {
        if (sscanf(tmp_env, "%d", &max_level) == 1) {
            int my_level = CBSDKD_LOGLVL_MAX - max_level;
            if (my_level <= 0) {
                my_level = CBSDKD_LOGLVL_ALL;
            }
            debugp->level = my_level;
        } else {
            debugp->level = CBSDKD_LOGLVL_WARN;
        }
    } else {
        debugp->level = CBSDKD_LOGLVL_WARN;
        return;
    }

    if (getenv(CBSDKD_DEBUG_ENV_COLOR_ENABLE)) {
        debugp->color = 1;
    } else {
        debugp->color = 0;
    }
}

void cbsdkd_logger(const cbsdkd_debug_st *debugp,
                         cbsdkd_loglevel_t level,
                         int line, const char *fn, const char *fmt, ...)
{
    va_list ap;
    const char *title_fmt = "", *reset_fmt = "", *line_fmt = "";

    if (debugp->level > level) {
        return;
    }

    if (debugp->color) {
        title_fmt = Color_title_fmt;
        reset_fmt = Color_reset_fmt;
        switch (level) {
        case CBSDKD_LOGLVL_CRIT:
        case CBSDKD_LOGLVL_ERROR:
            line_fmt = "\033[" _BRIGHT_FG ";" _FG _RED "m";
            break;

        case CBSDKD_LOGLVL_WARN:
            line_fmt = "\033[" _FG _YELLOW "m";
            break;

        case CBSDKD_LOGLVL_DEBUG:
        case CBSDKD_LOGLVL_TRACE:
            line_fmt = "\033[" _DIM_FG ";" _FG _WHITE "m";
            break;

        default:
            /* No color */
            line_fmt = "";
            title_fmt = "";
            break;
        }
    }

    va_start(ap, fmt);
    flockfile(debugp->out);
    fprintf(debugp->out,
            "[%s%s%s] " /*title, prefix, reset color*/
            "%s" /*line color format*/
            "%s:%d ", /*__func__, __LINE__*/
            title_fmt, debugp->prefix, reset_fmt,
            line_fmt,
            fn, line);

    vfprintf(debugp->out, fmt, ap);

    fprintf(debugp->out, "%s\n", reset_fmt);

    fflush(debugp->out);
    funlockfile(debugp->out);
    va_end(ap);
}

void cbsdkd_hex_dump(const void *data, size_t size)
{
    /* dumps size bytes of *data to stdout. Looks like:
     * [0000] 75 6E 6B 6E 6F 77 6E 20
     *                  30 FF 00 00 00 00 39 00 unknown 0.....9.
     * (in a single line of course)
     */

    unsigned char *p = (unsigned char *)data;
    unsigned char c;
    size_t n;

    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16 * 3 + 5] = {0};
    char charstr[16 * 1 + 5] = {0};

    for (n = 1; n <= size; n++) {
        if (n % 16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4lx",
                     (unsigned long)
                     ((size_t)p - (size_t)data));
        }

        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr) - strlen(hexstr) - 1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr) - strlen(charstr) - 1);

        if (n % 16 == 0) {
            /* line completed */
            printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if (n % 8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr) - strlen(hexstr) - 1);
            strncat(charstr, " ", sizeof(charstr) - strlen(charstr) - 1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        fprintf(stderr, "[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}
