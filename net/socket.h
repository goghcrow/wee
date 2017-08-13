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
#include <netdb.h>
#include <signal.h>

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
    int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("ERROR socket");
        exit(1);
    }
    socket_setNonblock(sockfd);
#else
    // linux 内核高版本可以直接设置 SOCK_NONBLOCK | SOCK_CLOEXEC,生一次fcntl系统调用
    int sockfd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("ERROR socket");
        exit(1);
    }
#endif

    int yes = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));

    // !! for server
    signal(SIGPIPE, SIG_IGN);
    return sockfd;
}

void socket_bind(int sockfd, const struct sockaddr *addr)
{
    if (bind(sockfd, addr, sizeof(*addr)) < 0)
    {
        close(sockfd);
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

// FIXME getpeer...



// port listen port for server, connect port for client
// server :: socket_createSync(NULL, 9999)
// client :: socket_createSync("www.google.com", 90)
int socket_createSync(char *host, char *port)
{
    bool isServer = host == NULL;

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP

    int status = getaddrinfo(host, port, &hints, &servinfo);
    if (status != 0)
    {
        fprintf(stderr, "ERROR getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    // 绑定到第一个能用的
    int sockfd;
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0)
        {
            perror("ERROR socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
        {
            perror("ERROR setsockopt SO_REUSEADDR");
            close(sockfd);
            exit(1);
        }
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) < 0)
        {
            perror("ERROR setsockopt TCP_NODELAY");
            close(sockfd);
            exit(1);
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) < 0)
        {

            perror("ERROR setsockopt SO_KEEPALIVE");
            close(sockfd);
            exit(1);
        }

        // SO_REUSEADDR needs to be set before bind().
        // However, not all options need to be set before bind(), or even before connect().
        if (isServer && bind(sockfd, p->ai_addr, p->ai_addrlen) < 0)
        {
            perror("ERROR bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        // 注意bind失败要close
        fprintf(stderr, "ERROR failed to socket and bind\n");
        close(sockfd);
        exit(1);
    }

    if (isServer)
    {
        // server 已经不用, client connect 仍然用
        freeaddrinfo(servinfo);
        signal(SIGPIPE, SIG_IGN);
        if (listen(sockfd, SOMAXCONN) < 0)
        {
            perror("ERROR listen");
            close(sockfd);
            exit(1);
        }
    }
    else
    {
        if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
        {
            perror("ERROR connect");
            freeaddrinfo(servinfo);
            close(sockfd);
            exit(1);
        }
    }

    return sockfd;
}

#endif