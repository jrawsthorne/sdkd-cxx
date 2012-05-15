#ifndef DEBUG_PLUS_PLUS_H_
#define DEBUG_PLUS_PLUS_H_

#include "debug.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace CBSdkd {

#define cbsdkd_log_trace(ctx, fmt, ...) \
    cbsdkd_logger(ctx, CBSDKD_LOGLVL_TRACE, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define log_trace(fmt, ...) \
    cbsdkd_log_trace(&this->cbsdkd__debugctx, fmt, ## __VA_ARGS__)
#define log_noctx_trace(fmt, ...) \
    cbsdkd_log_trace(&CBsdkd_Global_Debug_Context.cbsdkd__debugctx, \
    fmt, ## __VA_ARGS__)

#define cbsdkd_log_info(ctx, fmt, ...) \
    cbsdkd_logger(ctx, CBSDKD_LOGLVL_INFO, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define log_info(fmt, ...) \
    cbsdkd_log_info(&this->cbsdkd__debugctx, fmt, ## __VA_ARGS__)
#define log_noctx_info(fmt, ...) \
    cbsdkd_log_info(&CBsdkd_Global_Debug_Context.cbsdkd__debugctx, \
    fmt, ## __VA_ARGS__)

#define cbsdkd_log_debug(ctx, fmt, ...) \
    cbsdkd_logger(ctx, CBSDKD_LOGLVL_DEBUG, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define log_debug(fmt, ...) \
    cbsdkd_log_debug(&this->cbsdkd__debugctx, fmt, ## __VA_ARGS__)
#define log_noctx_debug(fmt, ...) \
    cbsdkd_log_debug(&CBsdkd_Global_Debug_Context.cbsdkd__debugctx, \
    fmt, ## __VA_ARGS__)

#define cbsdkd_log_warn(ctx, fmt, ...) \
    cbsdkd_logger(ctx, CBSDKD_LOGLVL_WARN, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define log_warn(fmt, ...) \
    cbsdkd_log_warn(&this->cbsdkd__debugctx, fmt, ## __VA_ARGS__)
#define log_noctx_warn(fmt, ...) \
    cbsdkd_log_warn(&CBsdkd_Global_Debug_Context.cbsdkd__debugctx, \
    fmt, ## __VA_ARGS__)

#define cbsdkd_log_error(ctx, fmt, ...) \
    cbsdkd_logger(ctx, CBSDKD_LOGLVL_ERROR, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define log_error(fmt, ...) \
    cbsdkd_log_error(&this->cbsdkd__debugctx, fmt, ## __VA_ARGS__)
#define log_noctx_error(fmt, ...) \
    cbsdkd_log_error(&CBsdkd_Global_Debug_Context.cbsdkd__debugctx, \
    fmt, ## __VA_ARGS__)

#define cbsdkd_log_crit(ctx, fmt, ...) \
    cbsdkd_logger(ctx, CBSDKD_LOGLVL_CRIT, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define log_crit(fmt, ...) \
    cbsdkd_log_crit(&this->cbsdkd__debugctx, fmt, ## __VA_ARGS__)
#define log_noctx_crit(fmt, ...) \
    cbsdkd_log_crit(&CBsdkd_Global_Debug_Context.cbsdkd__debugctx, \
    fmt, ## __VA_ARGS__)




class DebugContext {
public:
    DebugContext() : allocstr(NULL) {
        cbsdkd__debugctx.color = 1;
        cbsdkd__debugctx.level = CBSDKD_LOGLVL_WARN;
        cbsdkd__debugctx.out = stderr;
        cbsdkd__debugctx.initialized = 1;
        cbsdkd__debugctx.prefix = "CBSDKD";
    }

    void setLogPrefix(const std::string& prefix) {
        if (allocstr) {
            free(allocstr);
        }
        allocstr = (char*)calloc(1, prefix.size() + 1);
        memcpy(allocstr, prefix.data(), prefix.size());
        cbsdkd__debugctx.prefix = allocstr;
    }

    std::string getLogPrefix() {
        std::string ret = std::string(cbsdkd__debugctx.prefix);
        return ret;
    }

    virtual ~DebugContext() {
        if (allocstr && cbsdkd__debugctx.prefix == allocstr) {
            cbsdkd__debugctx.prefix = "";
            free(allocstr);
        }
    }

    cbsdkd_debug_st cbsdkd__debugctx;
private:
    char *allocstr;
};

extern DebugContext CBsdkd_Global_Debug_Context;

};

#endif /* DEGBUG_PLUS_PLUS_H_ */
