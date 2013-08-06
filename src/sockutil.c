#define SDKD_NO_CXX
#include "sdkd_internal.h"

sdkd_socket_t sdkd_start_listening(struct sockaddr_in *addr)
{
    sdkd_socket_t acceptfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int optval;
    unsigned int addrlen;

#ifdef _WIN32
    const char *optval_p = (const char*)&optval;
#else
    int *optval_p = &optval;
#endif

    if (acceptfd == -1) {
        return -1;
    }

    optval = 1;

    if (-1 == setsockopt(acceptfd, SOL_SOCKET, SO_REUSEADDR, optval_p, sizeof(int))) {
        closesocket(acceptfd);
        return -1;
    }

    if (-1 == bind(acceptfd, (struct sockaddr*)addr, sizeof(*addr))) {
        closesocket(acceptfd);
        return -1;
    }

    addrlen = sizeof(*addr);
    if (-1 == getsockname(acceptfd, (struct sockaddr*)addr, &addrlen)) {
        closesocket(acceptfd);
        return -1;
    }

    if (-1 == listen(acceptfd, 5)) {
        closesocket(acceptfd);
        return -1;
    }
    return acceptfd;
}

int sdkd_make_socket_nonblocking(int sockfd, int nonblocking)
{
#ifndef _WIN32
    int existing_flags = fcntl(sockfd, F_GETFL);

    if (existing_flags == -1) {
        perror("fcntl");
        return -1;
    }

    if (nonblocking) {
        existing_flags |= O_NONBLOCK;

    } else {
        existing_flags &= ~O_NONBLOCK;
    }

    return fcntl(sockfd, F_SETFL, existing_flags);

#else
    u_long argp = nonblocking;
    return ioctlsocket(sockfd, FIONBIO, &argp);
#endif
}

sdkd_socket_t sdkd_accept_socket(int acceptfd, struct sockaddr_in *saddr)
{
    sdkd_socket_t ret;
#ifdef _WIN32
    int addrlen;
#else
    socklen_t addrlen;
#endif

    addrlen = sizeof(*saddr);

    if (-1 == ( ret = accept(acceptfd,
                                (struct sockaddr*)saddr, &addrlen))) {
        printf("Accept() failed!\n");
        return -1;  // Nothing to do here..
    }
    return ret;

}

int sdkd_socket_errno(void)
{
#ifndef _WIN32
    return errno;
#else
    return WSAGetLastError();
#endif
}
