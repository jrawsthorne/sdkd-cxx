#ifndef SDKD_INTERNAL_H_
#define SDKD_INTERNAL_H_

#include <libcouchbase/couchbase.h>
#include <libcouchbase/api3.h>
#include <libcouchbase/views.h>
#include <libcouchbase/n1ql.h>
#include <libcouchbase/cbft.h>
#include "protocol_binary.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#define closesocket close
#define sdkd_millisleep(ms) usleep(ms * 1000)
#define sdkd_strdup strdup
#define SDKD_SOCK_EWOULDBLOCK EWOULDBLOCK
#define SDKD_SOCK_EINTR EINTR
#define PID_FILE "/var/run/sdkd-cpp.pid"
typedef int sdkd_socket_t;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#else
#include <windows.h>

#define suseconds_t lcb_int64_t
#define useconds_t lcb_uint64_t
#define __func__ __FUNCTION__
#define setenv(a,b,c) SetEnvironmentVariable(a,b)
#define sdkd_millisleep(ms) Sleep(ms)
typedef SOCKET sdkd_socket_t;
#define SDKD_SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SDKD_SOCK_EINTR WSAEINTR
#define sdkd_strdup _strdup /* avoid warnings */
#endif

/************************************************************************/
/* Common Includes                                                       */
/************************************************************************/
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include<stdlib.h>

#include "protostrings.h"

typedef enum {
    SD_INSERT,
    SD_GET,
    SD_EXISTS,
    SD_UPSERT,
    SD_REPLACE,
    SD_DELETE,
    SD_PUSHIN,
    SD_COUNTER,
    SD_MULTIMUTATE,
    SD_MULTILOOKUP
} SUBDOC_OP;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plain old C functions and symbols.
 */
void sdkd_set_ttl(unsigned seconds);
void sdkd_init_timer(void);
sdkd_socket_t sdkd_start_listening(struct sockaddr_in *addr);
int sdkd_make_socket_nonblocking(sdkd_socket_t sockfd, int nonblocking);
sdkd_socket_t sdkd_accept_socket(sdkd_socket_t acceptfd,
                                 struct sockaddr_in *saddr);
int sdkd_socket_errno(void);

lcb_io_opt_t sdkd_create_iops(void);

#ifdef _WIN32
int gettimeofday(struct timeval *tv, void *tz);
#endif

#ifdef __cplusplus
}
#endif



#ifndef SDKD_NO_CXX

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <memory>
#include <set>
#include <sstream>
#include <list>

#include <json/json.h>
#include "contrib/debug++.h"

#include "utils.h"
#include "Daemon.h"
#include "Thread.h"
#include "Error.h"
#include "Message.h"
#include "Request.h"
#include "Response.h"
#include "Dataset.h"
#include "ResultSet.h"
#include "Handle.h"
#include "IODispatch.h"
#include "UsageCollector.h"
#include "logging.h"

#include "views/viewopts.h"
#include "views/viewrow.h"
#include "Views.h"
#include "n1ql.h"
#include "subdoc.h"
#include "fts.h"

#endif /* SDKD_NO_CXX */


#endif
