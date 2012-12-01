/**
 * View extensions for libcouchbase.
 *
 * This contains functions to:
 *
 * o Build and validate view options
 * o Encode required view option values into JSON
 * o Encode required view option values into percent-encoding
 */

#ifndef LCB_VIEWOPTS_H_
#define LCB_VIEWOPTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libcouchbase/couchbase.h"

enum {
    /* encode the value in percent-encoding if needed */
    LCB_VOPT_F_PCTENCODE = 1<<0,

    /* option value should be treated as an int (and possibly coerced)
     * rather than a string
     */
    LCB_VOPT_F_OPTVAL_NUMERIC = 1<<1,

    LCB_VOPT_F_PASSTHROUGH = 1<<2,

    /* option value is constant. Don't free */
    LCB_VOPT_F_OPTVAL_CONSTANT = 1<<3,

    /* option name is constant. Don't free */
    LCB_VOPT_F_OPTNAME_CONSTANT = 1<<4,

    /* Option name is an integer constant, not a string */
    LCB_VOPT_F_OPTNAME_NUMERIC = 1<<5,
};


/**
 * This x-macro accepts three arguments:
 * (1) The 'base' constant name for the view option
 * (2) The string name (as is encoded into the URI)
 * (3) The expected type. These are used internally. Types are:
 *      'bool' - coerced into a 'true' or 'false' string
 *      'num' - coerced into a numeric string
 *      'string' - optionally percent-encoded
 *      'jval' - aliased to string, but means a JSON-encoded primitive or
 *          complex value
 *      'jarry' - a JSON array
 *      'onerror' - special type accepting the appropriate values (stop, continue)
 *      'stale' - special type accepting ('ok' (coerced if needed from true), 'false',
 *          and 'update_after')
 */
#define LCB_XVOPT \
    XX(DESCENDING, "descending", bool) \
    XX(ENDKEY, "endkey", jval) \
    XX(ENDKEY_DOCID, "endkey_docid", string) \
    XX(FULLSET, "full_set", bool) \
    XX(GROUP, "group", bool) \
    XX(GROUP_LEVEL, "group_level", num) \
    XX(INCLUSIVE_END, "inclusive_end", bool) \
    XX(KEYS, "keys", jarry) \
    XX(SINGLE_KEY, "key", jval) \
    XX(ONERROR, "on_error", onerror) \
    XX(REDUCE, "reduce", bool) \
    XX(STALE, "stale", stale) \
    XX(SKIP, "skip", num) \
    XX(LIMIT, "limit", num) \
    XX(STARTKEY, "startkey", jval) \
    XX(STARTKEY_DOCID, "startkey_docid", string) \
    XX(DEBUG, "debug", bool)

enum {
    LCB_VOPT_OPT_CLIENT_PASSTHROUGH = 0,

#define XX(b, str, type) \
    LCB_VOPT_OPT_##b,
    LCB_XVOPT
#undef XX
    _LCB_VOPT_OPT_MAX
};


typedef struct lcb_vopt_st {
    /* NUL-terminated option name */
    const char *optname;
    /* NUL-terminated option value */
    const char *optval;
    size_t noptname;
    size_t noptval;
    int flags;
} lcb_vopt_t;

/**
 * Properly initializes a view_option structure, checking (if requested)
 * for valid option names and inputs
 *
 * @param optobj an allocated but empty optobj structure
 *
 * @param option An option name.
 * This may be a string or something else dependent on the flags
 *
 * @param noption Size of an option (if the option name is a string)
 *
 * @param value The value for the option. This may be a string (defaul) or
 * something else depending on the flags. If a string, it must be UTF-8 compatible
 *
 * @param nvalue the sizeo of the value (if the value is a string).
 *
 * @param flags. A set of flags to specify for the conversion
 *
 * @param error_string An error string describing the details of why validation
 * failed. This is a constant string and should not be freed. Only valid if the
 * function does not return LCB_SUCEESS
 *
 * @return LCB_SUCCESS if the validation/conversion was a success, or an error
 * code (typically LCB_EINVAL) otherwise.
 *
 * If the operation succeeded, the option object should be cleaned up using
 * free_view_option which will clean up necessary fields within the structure.
 * As the actual structure is not allocated by the library, the library will
 * not free the structure.
 */
lcb_error_t
lcb_vopt_assign(lcb_vopt_t *optobj,
                const void *option,
                size_t noption,
                const void *value,
                size_t nvalue,
                int flags,
                char **error_string);

/**
 * Creates an array of options from a list of strings. The list should be
 * NULL terminated
 * @param optarray a pointer which will contain an array of vopts
 * @param noptions will contain the number of vopts
 * @param errstr - will contain a pointer to a string upon error
 * @param .. "key", "value" pairs
 * @return LCB_SUCCESS on success, error otherwise. All memory is freed on error;
 * otherwise the list must be freed using vopt_cleanup
 */
lcb_error_t
lcb_vopt_createv(lcb_vopt_t *optarray[],
                 size_t *noptions, char **errstr, ...);

/**
 * Cleans up a vopt structure. This does not free the structure, but does
 * free any allocated members in the structure's internal fields (if any(.
 */
void
lcb_vopt_cleanup(lcb_vopt_t *optobj);

/**
 * Convenience function to free a list of options. This is here since many
 * of the functions already require an lcb_vopt_st **
 *
 * @param options a list of options
 * @param noptions how many options
 * @param contiguous whether the pointer points to an array of pointers,
 * (i.e. where *(options[n]) dereferneces the structure, or a pointer to a
 * continuous block of memory, so that (*options)[n] dereferences
 */
void
lcb_vopt_cleanup_list(lcb_vopt_t ** options, size_t noptions,
                      int contiguous);


/**
 * Calculates the minimum size of the query portion of a buffer
 */
size_t
lcb_vqstr_calc_len(const lcb_vopt_t * const * options,
                   size_t noptions);

/**
 * Writes the query string to a buffer. The buffer must have enough space
 * as determined by vqstr_calc_len.
 * Returns how many bytes were actually written to the buffer
 */
size_t
lcb_vqstr_write(const lcb_vopt_t * const * options, size_t noptions,
                char *buf);


/**
 * Creates a proper URI query string for a view with its parameters.
 *
 * @param design the name of the design document
 * @param ndesign length of design name (-1 for nul-terminated)
 * @param view the name of the view to query
 * @param nview the length of the view name (-1 for nul-terminated)
 * @param options the view options for this query
 * @param noptions how many options
 *
 * @return an allocated string (via malloc) which may be used for the view
 * query. The string will be NUL-terminated so strlen may be called to obtain
 * the length.
 */
char *
lcb_vqstr_make_uri(const char *design, size_t ndesign,
                   const char *view, size_t nview,
                   const lcb_vopt_t * const * options,
                   size_t noptions);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LCB_VIEWOPTS_H_ */
