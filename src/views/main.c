#include <libcouchbase/couchbase.h>
#include "viewrow.h"
#include "viewopts.h"
#include <stdio.h>

#undef NDEBUG
#include <assert.h>
#include <sys/stat.h>


#define OV1(p) ((p)->v.v1)
#define OV0(p) ((p)->v.v0)

static void
my_vr_callback(lcb_vrow_ctx_t *ctx,
                  const void *cookie,
                  const lcb_vrow_datum_t *res)
{
    if (res->type == LCB_VROW_ERROR) {
        printf("Got parse error..\n");
        abort();
    }

    if (res->type == LCB_VROW_COMPLETE) {
        printf("Rows done. Metadata is here..\n");
        printf("%.*s\n",
               (int)res->ndata,
               res->data);

        return;
    }

    if (*res->data != '{' || res->data[res->ndata-1] != '}') {
        abort();
    }

    printf("== Row Callback == (n=%lu)\n", res->ndata);
}

static void
http_data_callback(lcb_http_request_t request,
                       lcb_INSTANCE *instance,
                       const void *cookie,
                       lcb_STATUS error,
                       const lcb_http_resp_t *resp)
{
    lcb_vrow_ctx_t *rctx = (lcb_vrow_ctx_t*)cookie;

    printf("Got callback..\n");
    printf("Got status %d\n", resp->v.v0.status);

    assert(error == LCB_SUCCESS);
    if (resp->v.v0.nbytes) {
        lcb_vrow_feed(rctx,
                      (const char*)resp->v.v0.bytes,
                      resp->v.v0.nbytes);
    } else {
        printf("No data.. hrm.\n");
    }
}

char *
get_view_buffer(const char *fname)
{
    FILE *fp = fopen(fname, "r");
    assert(fp);

    struct stat sb;
    int ret = fstat(fileno(fp), &sb);
    assert(ret != -1);

    char *buf = malloc(sb.st_size + 1);
    buf[sb.st_size] = '\0';
    fread(buf, 1, sb.st_size, fp);
    fclose(fp);

    return buf;
}

enum {
    OPTIX_STALE = 0,
    OPTIX_LIMIT,
    OPTIX_GROUP,
    OPTIX_DEBUG,
    _OPTIX_MAX
};

static void
schedule_http(lcb_INSTANCE *instance)
{
    lcb_http_cmd_t htcmd;
    lcb_http_request_t htreq;

    lcb_STATUS err;
    char *vqstr;
    char *errstr;
    size_t num_options;

    lcb_vopt_t *vopt_list, *vopt_ptarray[10];
    lcb_vrow_ctx_t *rctx = lcb_vrow_create();

    int ii;
    err = lcb_vopt_createv(&vopt_list,
                           &num_options,
                           &errstr,
                           "stale", "false",
                           "limit", "300",
                           "debug", "true",
                           NULL);

    assert(err == LCB_SUCCESS);
    assert(num_options);
    for (ii = 0; ii < num_options; ii++) {
        vopt_ptarray[ii] = vopt_list + ii;
    }

    vqstr = lcb_vqstr_make_uri("beer", -1,
                               "brewery_beers", -1,
                               (const lcb_vopt_t* const*)vopt_ptarray,
                               num_options);

    memset(&htcmd, 0, sizeof(htcmd));

    OV0(&htcmd).content_type = "application/json";
    OV0(&htcmd).path = vqstr;
    OV0(&htcmd).npath = strlen(vqstr);
    OV0(&htcmd).chunked = 1;
    OV0(&htcmd).method = LCB_HTTP_METHOD_GET;

    rctx->callback = my_vr_callback;
    lcb_set_http_data_callback(instance, http_data_callback);

    err = lcb_make_http_request(instance,
                                rctx,
                                LCB_HTTP_TYPE_VIEW,
                                &htcmd,
                                &htreq);
    assert(err == LCB_SUCCESS);
    printf("Waiting for request..\n");
    err = lcb_wait(instance);
    assert (err == LCB_SUCCESS);

//    printf("Got query string: %s\n", vqstr);
    free(vqstr);
    lcb_vopt_cleanup_list(&vopt_list, num_options, 1);
    free(vopt_list);
    lcb_vrow_free(rctx);

}

static lcb_INSTANCE
*create_handle(void)
{
    struct lcb_create_st ctor_opts;
    lcb_INSTANCE *instance = NULL;
    lcb_STATUS err;
    memset(&ctor_opts, 0, sizeof(ctor_opts));

    OV1(&ctor_opts).bucket = "beer-sample";
    OV1(&ctor_opts).user = "Administrator";
    OV1(&ctor_opts).passwd = "123456";
    OV1(&ctor_opts).host = "127.0.0.1:8091";
    if (LCB_SUCCESS != lcb_create(&instance, &ctor_opts)) {
        abort();
    }

    err = lcb_connect(instance);
    assert(err == LCB_SUCCESS);

    err = lcb_wait(instance);
    assert(err == LCB_SUCCESS);

    return instance;
}

static void
test_vopts(void)
{
    /**
     * Create some view options..
     */
    lcb_vopt_t opt_stale;
    lcb_vopt_t opt_onerr;
    lcb_vopt_t opt_limit;
    lcb_vopt_t opt_invalid;

    lcb_vopt_t *optlist[] = {
            &opt_stale,
            &opt_onerr,
            &opt_limit,
            &opt_invalid
    };
    const lcb_vopt_t * const * vl_const = (const lcb_vopt_t* const* )optlist;

    lcb_STATUS err;
    char *estr = NULL;
    int optval;
    int opttype;


    /**
     * Use constants (and avoid mistyping strings) for options which support it
     */
    opttype = LCB_VOPT_OPT_STALE;
    optval = 0;
    err = lcb_vopt_assign(&opt_stale,
                          &opttype,
                          0,
                          &optval,
                          0,
                          LCB_VOPT_F_OPTNAME_NUMERIC
                                  | LCB_VOPT_F_OPTVAL_NUMERIC,
                          &estr);

    assert (err == LCB_SUCCESS);

    /**
     * Strings!
     */
    err = lcb_vopt_assign(&opt_onerr,
                               "on_error", -1,
                               "continue", -1,
                               0,
                               &estr);

    assert (err == LCB_SUCCESS);
    optval = 10;

    /* mix them around */
    err = lcb_vopt_assign(&opt_limit,
                               "limit", -1,
                               &optval, 0,
                               LCB_VOPT_F_OPTVAL_NUMERIC,
                               &estr);

    assert (err == LCB_SUCCESS);

    /* Invalid options fail */
    err = lcb_vopt_assign(&opt_invalid,
                               "invalid_option_string", -1,
                               "invalid value", -1,
                               0, &estr);

    /* Try it again, with a special flag */
    assert(err == LCB_EINVAL);
    printf("%s\n", estr);

    /* Try it again, with a special flag */
    err = lcb_vopt_assign(&opt_invalid,
                          "invalid_option_string",
                          -1,
                          "[\"json\",\"encoded\"]",
                          -1,
                          LCB_VOPT_F_PASSTHROUGH | LCB_VOPT_F_PCTENCODE,
                          &estr);

    assert(err == LCB_SUCCESS);

    int num_options = sizeof(optlist) / sizeof(*optlist);
    printf("Estimated length: %d\n",
           lcb_vqstr_calc_len(vl_const, num_options));

    char buf[128] = { 0 };
    lcb_vqstr_write(vl_const, num_options, buf);
    printf("Got buffer %s\n", buf);

    char *auto_buf = lcb_vqstr_make_uri("design_doc", -1,
                                        "view_doc", -1,
                                        vl_const, num_options);
    printf("Got request URI: %s\n", auto_buf);
    free(auto_buf);

    lcb_vopt_cleanup_list(optlist, num_options, 0);
}

static void
test_refused(lcb_INSTANCE *handle)
{
    lcb_http_cmd_t cmd;
    lcb_http_request_t req;

    memset(&cmd, 0, sizeof(cmd));
    cmd.version = 1;
    OV1(&cmd).host = "127.0.0.1:2";
    OV1(&cmd).path = "/";
    OV1(&cmd).npath = 1;
    OV1(&cmd).content_type = "application/json";
    OV1(&cmd).method = LCB_HTTP_METHOD_GET;

    lcb_STATUS err;
    err = lcb_make_http_request(handle, NULL, LCB_HTTP_TYPE_RAW, &cmd, &req);
    assert(err == LCB_SUCCESS);
    lcb_wait(handle);
    printf("Wait done..\n");
}

static void
test_vrow_real(lcb_vrow_ctx_t *rctx)
{
    int rctx_allocated = rctx == NULL;

    if (!rctx) {
        rctx = lcb_vrow_create();
    }
    rctx->callback = my_vr_callback;

    char *view_out = get_view_buffer("views.out");
    size_t stream_len = strlen(view_out);
    char *view_last = view_out + stream_len, *view_cur = view_out;

    while (view_cur < view_last) {
        size_t cur_len = view_last - view_cur;
        if (cur_len > 3) {
            cur_len = 3;
        }
        lcb_vrow_feed(rctx, view_cur, cur_len);
        view_cur += cur_len;
    }

    size_t meta_len;
    const char *meta_data = lcb_vrow_get_meta(rctx, &meta_len);
    printf("Sekeleton: %.*s\n", meta_len, meta_data);

    if (rctx_allocated) {
        lcb_vrow_free(rctx);
    }
    free(view_out);
}

static void
test_vrow(void)
{
    int ii;
    lcb_vrow_ctx_t *rctx = lcb_vrow_create();
    for (ii = 0; ii < 10; ii++) {
        test_vrow_real(rctx);
        lcb_vrow_reset(rctx);
    }
    lcb_vrow_free(rctx);
}

int main(void)
{
    lcb_INSTANCE *instance = create_handle();
    test_refused(instance);
//    schedule_http(instance);
    lcb_destroy(instance);
//
    test_vrow();
//    test_vopts();
//
    return 0;
}
