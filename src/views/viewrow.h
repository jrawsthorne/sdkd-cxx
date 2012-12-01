#ifndef LCB_VIEWROW_H_
#define LCB_VIEWROW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "jsonsl.h"
#include <libcouchbase/couchbase.h>

typedef struct lcb_rows_ctx_st lcb_vrow_ctx_t;

typedef enum {
    /**
     * This is a row of view data. You can parse this as JSON from your
     * favorite decoder/converter
     */
    LCB_VROW_ROW,

    /**
     * All the rows have been returned. In this case, the data is the 'meta'.
     * This is a valid JSON payload which was returned from the server.
     * The "rows" : [] array will be empty.
     */
    LCB_VROW_COMPLETE,

    /**
     * A JSON parse error occured. The payload will contain string data. This
     * may be JSON (but this is not likely).
     * The callback will be delivered twice. First when the error is noticed,
     * and second at the end (instead of a COMPLETE callback)
     */
    LCB_VROW_ERROR
} lcb_vrow_type_t;

typedef struct {
    /** The type of data encapsulated */
    lcb_vrow_type_t type;

    /** string data */
    const char *data;

    /** length */
    size_t ndata;

} lcb_vrow_datum_t;

typedef void (*lcb_vrow_callback_t)(lcb_vrow_ctx_t *ctx,
        const void *cookie,
        const lcb_vrow_datum_t *resp);


/**
 * Do we always need to always make these lame structures?
 */
typedef struct {
    char *s;
    size_t len;
    size_t alloc;
} lcb_vrow_buffer;


struct lcb_rows_ctx_st {
    /* jsonsl parser */
    jsonsl_t jsn;

    /* jsonpointer match object */
    jsonsl_jpr_t jpr;

    /* buffer containing the skeleton */
    lcb_vrow_buffer meta_buf;

    /* scratch/read buffer */
    lcb_vrow_buffer current_buf;

    /* last hash key */
    lcb_vrow_buffer last_hk;

    /* flags. This should be an int with a bunch of constant flags */
    int have_error;
    int initialized;
    int meta_complete;

    unsigned rowcount;

    /* absolute position offset corresponding to the first byte in current_buf */
    size_t min_pos;

    /* minimum (absolute) position to keep */
    size_t keep_pos;

    /**
     * size of the metadata header chunk (i.e. everything until the opening
     * bracket of "rows" [
     */
    size_t header_len;

    /**
     * Position of last row returned. If there are no subsequent rows, this
     * signals the beginning of the metadata trailer
     */
    size_t last_row_endpos;

    /**
     * User stuff:
     */

    /* wrapped cookie */
    void *user_cookie;

    /* callback to invoke */
    lcb_vrow_callback_t callback;

};

/**
 * Creates a new vrow context object.
 * You must set callbacks on this object if you wish it to be useful.
 * You must feed it data (calling vrow_feed) as well. The data may be fed
 * in chunks and callbacks will be invoked as each row is read.
 */
lcb_vrow_ctx_t*
lcb_vrow_create(void);

#define lcb_vrow_set_callback(vr, cb) vr->callback = cb

#define lcb_vrow_set_cookie(vr, cookie) vr->user_cookie = cookie


/**
 * Resets the context to a pristine state. Callbacks and cookies are kept.
 * This may be more efficient than allocating/freeing a context each time
 * (as this can be expensive with the jsonsl structures)
 */
void
lcb_vrow_reset(lcb_vrow_ctx_t *ctx);

/**
 * Frees a vrow object created by vrow_create
 */
void
lcb_vrow_free(lcb_vrow_ctx_t *ctx);

/**
 * Feeds data into the vrow. The callback may be invoked multiple times
 * in this function. In the context of normal lcb usage, this will typically
 * be invoked from within an http_data_callback.
 */
void
lcb_vrow_feed(lcb_vrow_ctx_t *ctx, const char *data, size_t ndata);

/**
 * Gets the metadata from the vrow
 */
const char *
lcb_vrow_get_meta(lcb_vrow_ctx_t *ctx, size_t *len);

/**
 * Gets a chunk of data from the vrow. There is no telling what the format
 * of the contained data will be; thus there is no guarantee that it will be
 * parseable as complete JSON.
 *
 * This is mainly useful for debugging non-success view responses
 */
const char *
lcb_vrow_get_raw(lcb_vrow_ctx_t *ctx, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* LCB_VIEWROW_H_ */
