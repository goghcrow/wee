#ifndef SOCKET_H
#define SOCKET_H

#include <sys/socket.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <assert.h>

typedef struct sockaddr SA;

struct sockaddr socket_createInetAddrIpPort(char *ip, uint16_t port)
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    assert(inet_pton(AF_INET, ip, &addr));
    // addr.sin_addr.s_addr = htonl(ip);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    return *(struct sockaddr *)(&addr);
}

struct sockaddr socket_createInetAddrPort(uint16_t port, bool loopback_only)
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    // loopback 127.0.0.1      any 0.0.0.0
    in_addr_t ip = loopback_only ? INADDR_LOOPBACK : INADDR_ANY;
    addr.sin_addr.s_addr = htonl(ip);
    addr.sin_port = htons(port);
    return *(struct sockaddr *)(&addr);
}

void socket_setNonblock(int sockfd)
{
    int flag = fcntl(sockfd, F_GETFL, 0);
    if (flag == -1)
    {
        return;
    }
    fcntl(sockfd, F_SETFL, flag | O_NONBLOCK | O_CLOEXEC);
}

int socket_create()
{
#ifdef __APPLE__
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("ERROR socket");
        exit(1);
    }
    socket_setNonblock(sockfd);
#else
    // linux 内核高版本可以直接设置 SOCK_NONBLOCK | SOCK_CLOEXEC,生一次fcntl系统调用
    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("ERROR socket");
        exit(1);
    }
#endif

    int optval = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, (socklen_t)sizeof(optval));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t)sizeof(optval));
#ifdef SO_REUSEPORT
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, (socklen_t)sizeof(optval));
#endif
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, (socklen_t)sizeof(optval));

    signal(SIGPIPE, SIG_IGN);
    return sockfd;
}

void socket_bind(int sockfd, const struct sockaddr *addr)
{
    if (bind(sockfd, addr, sizeof(*addr)) < 0)
    {
        perror("ERROR bind");
        exit(1);
    }
}

void socket_listen(int sockfd)
{
    // SOMAXCONN net.core.somaxconn linux 最大backlog值
    if (listen(sockfd, SOMAXCONN) < 0)
    {
        perror("ERROR listen");
        exit(1);
    }
}

int socket_accept(int sockfd, struct sockaddr *addr)
{
    int addr_len = sizeof(*addr);
#ifdef __APPLE__
    int connfd = accept(sockfd, addr, (socklen_t *)&addr_len);
    socket_setNonblock(connfd);
#else
    int connfd = accept4(sockfd, addr, (socklen_t *)addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    if (connfd < 0)
    {
        int savedErrno = errno;
        switch (savedErrno)
        {
        case EAGAIN:
        case ECONNABORTED:
        case EINTR:
        case EPROTO:
        case EPERM:
        case EMFILE:
            errno = savedErrno;
            break;
        case EBADF:
        case EFAULT:
        case EINVAL:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case ENOTSOCK:
        case EOPNOTSUPP:
            perror("ERROR accept, unexpected error");
            exit(1);
            break;
        default:
            perror("ERROR accept, unknown error");
            exit(1);
            break;
        }
    }
    return connfd;
}

int socket_connect(int sockfd, const struct sockaddr *addr)
{
    return connect(sockfd, addr, sizeof(*addr));
}

ssize_t socket_read(int sockfd, void *buf, size_t count)
{
    return read(sockfd, buf, count);
}

ssize_t socket_readv(int sockfd, const struct iovec *iov, int iovcnt)
{
    return readv(sockfd, iov, iovcnt);
}

ssize_t socket_write(int sockfd, const void *buf, size_t count)
{
    return write(sockfd, buf, count);
}

void socket_close(int sockfd)
{
    if (close(sockfd) < 0)
    {
        perror("ERROR close");
        exit(1);
    }
}

void socket_shutdownWrite(int sockfd)
{
    if (shutdown(sockfd, SHUT_WR) < 0)
    {
        perror("ERROR shutdown");
        exit(1);
    }
}

int socket_getError(int sockfd)
{
    int optval;
    int len = sizeof(optval);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, (socklen_t *)&len) < 0)
    {
        return errno;
    }
    else
    {
        return optval;
    }
}

#endif