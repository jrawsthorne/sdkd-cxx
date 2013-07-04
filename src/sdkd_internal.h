#ifndef SDKD_INTERNAL_H_
#define SDKD_INTERNAL_H_

#include <libcouchbase/couchbase.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

#define closesocket close
#define sdkd_millisleep(ms) usleep(ms * 1000)
#define SDKD_SOCK_EWOULDBLOCK EWOULDBLOCK
#define SDKD_SOCK_EINTR EINTR
typedef int sdkd_socket_t;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#else
#include <windows.h>

#define suseconds_t lcb_uint64_t
#define __func__ __FUNCTION__
#define setenv(a,b,c) SetEnvironmentVariable(a,b)
#define sdkd_millisleep(ms) Sleep(ms)
typedef SOCKET sdkd_socket_t;
#define SDKD_SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SDKD_SOCK_EINTR WSAEINTR
#endif

/************************************************************************/
/* Common Includes                                                       */
/************************************************************************/
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

#include "protostrings.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plain old C functions and symbols.
 */
void sdkd_set_ttl(unsigned seconds);
void sdkd_init_timer(void);
sdkd_socket_t sdkd_start_listening(struct sockaddr_in *addr);
int sdkd_make_socket_nonblocking(int sockfd, int nonblocking);
sdkd_socket_t sdkd_accept_socket(int acceptfd, struct sockaddr_in *saddr);
int sdkd_socket_errno(void);

lcb_io_opt_t sdkd_create_iops(void);

#ifdef _WIN32
int gettimeofday(struct timeval *tv, void *tz);
#endif


extern const char *SDKD_Conncache_Path;
extern int SDKD_No_Persist;

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
#include "Thread.h"
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

#endif /* SDKD_NO_CXX */


#endif
