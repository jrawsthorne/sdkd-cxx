/**
 * Streaming view row implementation
 */
#include "jsonsl.h"
#include "libcouchbase/couchbase.h"
#include "viewopts.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef _WIN32

#ifndef va_copy
  #define va_copy(a,b) (a=b)
#endif /* va_copy */

#ifndef strncasecmp
  #define strncasecmp _strnicmp
#endif /* strncasecmp */

#endif /* _WIN32 */

static char *my_strndup(const char *a, int n)
{
    char *ret = (char*)malloc(n);
    if (!ret) {
        return NULL;
    }
    strncpy(ret, a, n);
    return ret;
}

typedef struct view_param_st view_param;


typedef lcb_STATUS (*view_param_handler)
        (view_param *param,
                struct lcb_vopt_st *optobj,
                const void *value,
                size_t nvalue,
                int flags,
                char **error);

struct view_param_st {
    int itype;
    const char *param;
    view_param_handler handler;
};

#define DECLARE_HANDLER(name) \
    static lcb_STATUS\
        name(\
            view_param *p, \
            lcb_vopt_t *optobj, \
            const void *value, \
            size_t nvalue, \
            int flags, \
            char **error);

DECLARE_HANDLER(bool_param_handler);
DECLARE_HANDLER(num_param_handler);
DECLARE_HANDLER(string_param_handler);
DECLARE_HANDLER(stale_param_handler);
DECLARE_HANDLER(onerror_param_handler);

#define jval_param_handler string_param_handler
#define jarry_param_handler string_param_handler

#undef DECLARE_HANDLER

static view_param Recognized_View_Params[] = {
#define XX(b, str, hbase) \
{ LCB_VOPT_OPT_##b, str, hbase##_param_handler },
        LCB_XVOPT
#undef XX
        { 0, NULL, NULL }
};


/**
 * Sets the option structure's value fields. Should be called only with
 * user input, and only if flags & NUMERIC == 0
 */
static void set_user_string(struct lcb_vopt_st *optobj,
                            const void *value,
                            size_t nvalue,
                            int flags)
{
    optobj->noptval = nvalue;
    if (flags & LCB_VOPT_F_OPTVAL_CONSTANT) {
        optobj->optval = (const char*)value;
        optobj->flags |= LCB_VOPT_F_OPTVAL_CONSTANT;
    } else {
        optobj->optval = my_strndup((const char*)value, nvalue);
        optobj->flags &= (~LCB_VOPT_F_OPTVAL_CONSTANT);
    }
}

/**
 * Callbacks/Handlers for various parameters
 */
static lcb_STATUS
bool_param_handler(view_param *param,
                  struct lcb_vopt_st *optobj,
                  const void *value,
                  size_t nvalue,
                  int flags,
                  char **error)
{
    /**
     * Try real hard to get a boolean value
     */
    int bval = -1;
    *error = NULL;

    if (flags & LCB_VOPT_F_OPTVAL_NUMERIC) {
        bval = *(int*)value;

    } else {
        if (strncasecmp(
                "true", (const char*)value, nvalue) == 0) {
            bval = 1;
        } else if (strncasecmp(
                "false", (const char*)value, nvalue) == 0) {
            bval = 0;
        } else {
            *error = "String must be either 'true' or 'false'";
            return LCB_ERR_INVALID_ARGUMENT;
        }
    }

    optobj->optval = bval ? "true" : "false";
    optobj->noptval = strlen(optobj->optval);
    optobj->flags |= LCB_VOPT_F_OPTVAL_CONSTANT;

    return LCB_SUCCESS;
}

static lcb_STATUS
num_param_handler(view_param *param,
        struct lcb_vopt_st *optobj,
        const void *value,
        size_t nvalue,
        int flags,
        char **error)
{
    if (flags & LCB_VOPT_F_OPTVAL_NUMERIC) {
        char *numbuf = malloc(128); /* should be enough */

        /* assuming ints never reach this large in my lifetime */
#ifdef _WIN32
        optobj->noptval = _snprintf(numbuf, 128, "%d", *(int*)value);
#else
        optobj->noptval = snprintf(numbuf, 128, "%d", *(int*)value);
#endif
        optobj->optval = numbuf;

        /* clear the 'constant' flag */
        optobj->flags &= (~LCB_VOPT_F_OPTVAL_CONSTANT);
        return LCB_SUCCESS;

    } else {
        const char *istr = (const char*)value;
        if (nvalue == -1) {
            nvalue = strlen(istr);
        }

        do {

            unsigned int ii;
            if (!nvalue) {
                *error = "Received an empty string";
                return LCB_ERR_INVALID_ARGUMENT;
            }

            for (ii = 0; ii < nvalue; ii++) {
                if (!isdigit(istr[ii])) {
                    *error = "String must consist entirely of digits";
                    return LCB_ERR_INVALID_ARGUMENT;
                    break;
                }
            }
        } while (0);

        set_user_string(optobj, value, nvalue, flags);

    }

    return 0;
}


/**
 * This is the logic used in PHP's urlencode
 */
static int needs_pct_encoding(char c)
{
    if (c >= 'a' && c <= 'z') {
        return 0;
    }
    if (c >= 'A' && c <= 'Z') {
        return 0;
    }
    if (c >= '0' && c <= '9') {
        return 0;
    }

    if (c == '-' || c == '_' || c == '.') {
        return 0;
    }
    return 1;
}

/**
 * Encodes a string into percent encoding. It is assumed dest has enough
 * space.
 *
 * Returns the effective length of the string.
 */
static size_t do_pct_encode(char *dest, const char *src, size_t nsrc)
{
    char *p_dst = dest;
    size_t ret;
    int ii;
    for (ii = 0; ii < nsrc; ii++) {
        if (!needs_pct_encoding(src[ii])) {
            *p_dst = src[ii];
            p_dst++;
            continue;
        }

        sprintf(p_dst, "%%%02X", (unsigned char)src[ii]);
        p_dst += 3;
    }

    ret = p_dst - dest;
    return ret;
}

static lcb_STATUS
string_param_handler(view_param *p,
                    struct lcb_vopt_st *optobj,
                    const void *value,
                    size_t nvalue,
                    int flags,
                    char **error)
{
    if (flags & LCB_VOPT_F_OPTVAL_NUMERIC) {
        *error = "Option requires a string value";
        return LCB_ERR_INVALID_ARGUMENT;
    }

    if ( (flags & LCB_VOPT_F_PCTENCODE) == 0 ) {
        /* determine if we need to encode anything as a percent */
        set_user_string(optobj, value, nvalue, flags);
        return LCB_SUCCESS;

    } else {
        size_t needed_size = 0;
        const char *str = (const char*)value;
        int ii;

        for (ii = 0; ii < nvalue; ii++) {
            if (needs_pct_encoding(str[ii])) {
                needed_size += 3;
            } else {
                needed_size++;
            }
        }

        if (needed_size == nvalue) {
            set_user_string(optobj, value, nvalue, flags);
            return LCB_SUCCESS;
        }

        optobj->optval = malloc(needed_size + 1);
        ((char*)(optobj->optval))[needed_size] = '\0';
        optobj->noptval = do_pct_encode((char*)optobj->optval, str, nvalue);
    }
    return LCB_SUCCESS;
}

static lcb_STATUS
stale_param_handler(view_param *p,
                   struct lcb_vopt_st *optobj,
                   const void *value,
                   size_t nvalue,
                   int flags,
                   char **error)
{
    /**
     * See if we can get it to be a true/false param
     */
    optobj->flags |= LCB_VOPT_F_OPTVAL_CONSTANT;
    if (bool_param_handler(p, optobj, value, nvalue, flags, error)
            == LCB_SUCCESS) {

        if (*optobj->optval == 't') {
            optobj->optval = "ok";
            optobj->noptval = sizeof("ok")-1;
        }
        return LCB_SUCCESS;
    }

    if (strncasecmp("update_after", (const char*)value, nvalue) == 0) {
        optobj->optval = "update_after";
        optobj->noptval = sizeof("update_after")-1;
        return LCB_SUCCESS;

    } else if (strncasecmp("ok", (const char*)value, nvalue) == 0) {
        optobj->optval = "ok";
        optobj->noptval = sizeof("ok")-1;
        return LCB_SUCCESS;
    }

    *error = "stale must be a boolean or the string 'update_after'";
    return LCB_ERR_INVALID_ARGUMENT;
}

static lcb_STATUS
onerror_param_handler(view_param *param,
                     struct lcb_vopt_st *optobj,
                     const void *value,
                     size_t nvalue,
                     int flags,
                     char **error)
{
    *error = "on_error must be one of 'continue' or 'stop'";
    optobj->flags |= LCB_VOPT_F_OPTVAL_CONSTANT;

    if (strncasecmp(
            "stop", (const char*)value, nvalue) == 0) {
        optobj->optval = "stop";
        optobj->noptval = sizeof("stop")-1;

    } else if (
            strncasecmp(
                    "continue", (const char*)value, nvalue) == -0) {
        optobj->optval = "continue";
        optobj->noptval = sizeof("continue")-1;

    } else {
        return -1;
    }

    return LCB_SUCCESS;
}


static view_param*
find_view_param(const void *option, size_t noption, int flags)
{
    view_param *ret;
    for (ret = Recognized_View_Params; ret->param; ret++) {
        if (flags & LCB_VOPT_F_OPTNAME_NUMERIC) {
            if ( *(int*)option == ret->itype ) {
                return ret;
            }
        } else {
            if (strncmp((const char*)option, ret->param, noption) == 0) {
                return ret;
            }
        }
    }
    return NULL;
}

lcb_STATUS
lcb_vopt_assign(struct lcb_vopt_st *optobj,
                     const void *option,
                     size_t noption,
                     const void *value,
                     size_t nvalue,
                     int flags,
                     char **error_string)
{
    view_param *vparam;

    if (nvalue == -1) {
        nvalue = strlen((char*)value);
    }
    if (noption == -1) {
        noption = strlen((char*)option);
    }

    if ((flags & LCB_VOPT_F_OPTVAL_NUMERIC) == 0 && nvalue == 0) {
        *error_string = "Missing value length";
        return LCB_ERR_INVALID_ARGUMENT;
    }

    if ((flags & LCB_VOPT_F_OPTNAME_NUMERIC) == 0 && noption == 0) {
        *error_string = "Missing option name length";
        return LCB_ERR_INVALID_ARGUMENT;
    }

    if (flags & LCB_VOPT_F_PASSTHROUGH) {
        if (flags & LCB_VOPT_F_OPTNAME_NUMERIC) {
            *error_string = "Can't use passthrough with option constants";
            return LCB_ERR_INVALID_ARGUMENT;
        }
        optobj->optname = my_strndup((const char*)option, noption);
        optobj->noptname = noption;
        if (flags & LCB_VOPT_F_OPTVAL_NUMERIC) {
            return num_param_handler(NULL, optobj, value, nvalue, flags,
                                     error_string);
        } else {
            return string_param_handler(NULL, optobj, value, nvalue, flags,
                                        error_string);
        }
        /* not reached */
    }

    optobj->flags = flags;

    vparam = find_view_param(option, noption, flags);
    if (!vparam) {
        *error_string = "Unrecognized option";
        return LCB_ERR_INVALID_ARGUMENT;
    }

    if (flags & LCB_VOPT_F_OPTNAME_NUMERIC) {
        optobj->optname = vparam->param;
        optobj->noptname = strlen(vparam->param);
        optobj->flags |= LCB_VOPT_F_OPTNAME_CONSTANT;
    } else {

        optobj->noptname = noption;
        /* are we a literal or constant? */
        if (flags & LCB_VOPT_F_OPTNAME_CONSTANT) {
            optobj->optname = (const char*)option;

        } else {
            optobj->optname = my_strndup((const char*)option, noption);
        }
    }

    return vparam->handler(vparam, optobj, value, nvalue, flags, error_string);
}

void
lcb_vopt_cleanup(lcb_vopt_t *optobj)
{
    if (optobj->optname) {
        if ((optobj->flags & LCB_VOPT_F_OPTNAME_CONSTANT) == 0) {
            free((void*)optobj->optname);
        }
    }
    if (optobj->optval) {
        if ((optobj->flags & LCB_VOPT_F_OPTVAL_CONSTANT) == 0) {
            free((void*)optobj->optval);
        }
    }

    memset(optobj, 0, sizeof(*optobj));
}

void
lcb_vopt_cleanup_list(lcb_vopt_t ** options, size_t noptions,
                      int is_continguous)
{
    int ii;
    for (ii = 0; ii < noptions; ii++) {
        if (is_continguous) {
            lcb_vopt_cleanup((*options) + ii);
        } else {
            lcb_vopt_cleanup(options[ii]);
        }
    }
}

size_t
lcb_vqstr_calc_len(const lcb_vopt_t * const * options, size_t noptions)
{
    size_t ret = 1; /* for the '?' */
    int ii;
    for (ii = 0; ii < noptions; ii++) {
        const lcb_vopt_t *curopt = options[ii];
        ret += curopt->noptname;
        ret += curopt->noptval;
        /* add two for '&' and '=' */
        ret += 2;
    }

    return ret + 1;
}

size_t
lcb_vqstr_write(const lcb_vopt_t * const * options,
                size_t noptions,
                char *buf)
{
    int ii;
    char *bufp = buf;

    *bufp = '?';
    bufp++;

    for (ii = 0; ii < noptions; ii++) {
        const lcb_vopt_t *curopt = options[ii];
        memcpy(bufp, curopt->optname, curopt->noptname);
        bufp += curopt->noptname;

        *bufp = '=';
        bufp++;

        memcpy(bufp, curopt->optval, curopt->noptval);
        bufp += curopt->noptval;

        *bufp = '&';
        bufp++;
    }
    bufp--; /* trailing '&' */
    *bufp = '\0';
    return bufp - buf;
}

/**
 * Convenience function to make a view URI.
 */
char *
lcb_vqstr_make_optstr(const lcb_vopt_t * const * options,
                   size_t noptions)
{
    size_t needed_len;
    char *buf;

    needed_len = lcb_vqstr_calc_len(options, noptions);

    buf = malloc(needed_len);

    lcb_vqstr_write(options, noptions, buf);
    return buf;
}

lcb_STATUS
lcb_vopt_createv(lcb_vopt_t *optarray[], size_t *noptions, char **errstr, ...)
{
    /* two lists, first to determine how much to allocated */
    va_list ap_orig, ap_calc;
    char *strp;
    lcb_STATUS err = LCB_SUCCESS;
    int curix;

    va_start(ap_orig, errstr);
    va_copy(ap_calc, ap_orig);

    *noptions = 0;
    while ((strp = va_arg(ap_calc, char*))) {
        (*noptions)++;
    }

    va_end(ap_calc);

    if (*noptions % 2 || noptions == 0) {
        *errstr = "Got zero or odd number of arguments";
        return LCB_ERR_INVALID_ARGUMENT;
    }


    *optarray = calloc(*noptions, sizeof(**optarray));
    *noptions /= 2;
    curix = 0;

    while ((strp = va_arg(ap_orig, char*))) {

        char *value = va_arg(ap_orig, char*);
        err = lcb_vopt_assign((*optarray) + curix,
                              strp, -1,
                              value, -1,
                              0,
                              errstr);

        curix++;
        if (err != LCB_SUCCESS) {
            break;
        }

    }

    va_end(ap_orig);

    if (err != LCB_SUCCESS) {
        int ii;
        for (ii = 0; ii < curix; ii++) {
            lcb_vopt_cleanup((*optarray) + ii);
        }
        free(*optarray);
    }
    return err;
}
