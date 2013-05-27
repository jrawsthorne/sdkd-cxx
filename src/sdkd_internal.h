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
#include <json/json.h>
#include "contrib/debug++.h"
#include "utils.h"
#include "protostrings.h"

#include "Error.h"
#include "Message.h"
#include "Request.h"
#include "Response.h"
#include "Dataset.h"
#include "ResultSet.h"
#include "Handle.h"
#include "IODispatch.h"

#include "views/viewopts.h"
#include "views/viewrow.h"
#include "Views.h"


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
