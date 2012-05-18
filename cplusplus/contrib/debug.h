#ifndef CBSDKD_DEBUG_H
#define CBSDKD_DEBUG_H

#include <stdio.h>
#define CBSDKD_DEBUG 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    CBSDKD_LOGLVL_ALL = 0,
    CBSDKD_LOGLVL_TRACE,
    CBSDKD_LOGLVL_DEBUG,
    CBSDKD_LOGLVL_INFO,
    CBSDKD_LOGLVL_WARN,
    CBSDKD_LOGLVL_ERROR,
    CBSDKD_LOGLVL_CRIT,
    CBSDKD_LOGLVL_NONE
} cbsdkd_loglevel_t;

#define CBSDKD_LOGLVL_MAX CBSDKD_LOGLVL_CRIT

typedef struct {
    /*The 'title'*/
    const char *prefix;

    /*The minimum level allowable*/
    cbsdkd_loglevel_t level;

    /*Whether color is enabled*/
    int color;

    /*Output stream*/
    FILE *out;

    /*Set internally when this structure has been initialized*/
    int initialized;
} cbsdkd_debug_st;

/* Environment variables to control setting debug parameters */

/* If set to an integer, the integer is taken as the minimum allowable
 * output level.  If set to -1, then all levels are enabled
 */
#define CBSDKD_DEBUG_ENV_ENABLE "CBSDKD_DEBUG"

/*
 * Format log messages by color coding them according to their severity
 * using ANSI escape sequences
 */
#define CBSDKD_DEBUG_ENV_COLOR_ENABLE "CBSDKD_DEBUG_COLORS"

/**
 * this structure contains a nice title and optional level threshold.
 * do not instantiate or access explicitly. Use provided functions/macros
 */

/**
 * Core logging function
 */
void cbsdkd_logger(const cbsdkd_debug_st *logparams,
                         cbsdkd_loglevel_t level,
                         int line,
                         const char *fn,
                         const char *fmt, ...);


/**
 * print a hex dump of data
 */
void cbsdkd_hex_dump(const void *data, size_t nbytes);

#define CBSDKD_LOG_IMPLICIT(debugp, lvl_base, fmt, ...) \
        cbsdkd_logger(debugp, CBSDKD_LOGLVL_ ## lvl_base, \
                            __LINE__, __func__, \
                            fmt, ## __VA_ARGS__)


#define CBSDKD_LOG_EXPLICIT(instance, lvl_base, fmt, ...) \
        CBSDKD_LOG_IMPLICIT(instance->debug, lvl_base, fmt, ## __VA_ARGS__ )


/**
 * the following functions send a message of the specified level to
 * the debug logging system. These are noop if cbsdkd was not
 * compiled with debugging.
 */

extern
cbsdkd_loglevel_t cbsdkd_Default_Log_Level;



#ifndef CBSDKD_DEBUG_STATIC_INIT
#define CBSDKD_DEBUG_STATIC_INIT
#endif /*CBSDKD_DEBUG_STATIC_INIT*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CBSDKD_DEBUG_H */
