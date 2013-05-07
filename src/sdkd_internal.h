#ifndef SDKD_INTERNAL_H_
#define SDKD_INTERNAL_H_

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <memory>
#include <set>

// TODO: Support windows?

#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <libcouchbase/couchbase.h>
#include "lcb_10_compat.h"
#include <json/json.h>
#include "contrib/debug++.h"
#include "utils.h"

#ifdef LCB_VERSION
#define SDKD_HAVE_VIEW_SUPPORT
#endif

#include "protostrings.h"

#include "Error.h"
#include "Message.h"
#include "Request.h"
#include "Response.h"
#include "Dataset.h"
#include "ResultSet.h"
#include "Handle.h"
#include "IODispatch.h"

#ifdef SDKD_HAVE_VIEW_SUPPORT
#include "views/viewopts.h"
#include "views/viewrow.h"
#include "Views.h"


#else
#define SDKD_INIT_VIEWS()
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Cancellation/TTL. Not thread-safe */
void sdkd_set_ttl(unsigned seconds);
void sdkd_init_timer(void);

extern const char *SDKD_Conncache_Path;
extern int SDKD_No_Persist;

#ifdef __cplusplus
}
#endif

#endif
