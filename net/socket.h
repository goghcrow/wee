#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "sa.h"

// 快速创建server与client
int socket_client(char *host, char *port);
int socket_server(char *port);

// 辅助函数
int socket_create();
bool socket_bind(int sockfd, const union sockaddr_all *addr, socklen_t addrlen);
bool socket_listen(int sockfd);
int socket_accept(int sockfd, union sockaddr_all *addr, socklen_t *addrlen);
bool socket_connect(int sockfd, const union sockaddr_all *addr, socklen_t addrlen);
ssize_t socket_read(int sockfd, void *buf, size_t count);
ssize_t socket_readv(int sockfd, const struct iovec *iov, int iovcnt);
ssize_t socket_write(int sockfd, const void *buf, size_t count);
void socket_close(int sockfd);
void socket_shutdownWrite(int sockfd);
int socket_getError(int sockfd);
// FIXME gethostname
// FIXME getpeername

// 阻塞版本
int socket_clientSync(char *host, char *port);
int socket_serverSync(char *port);
int socket_createSync();
int socket_acceptSync(int sockfd, union sockaddr_all *addr, socklen_t *addrlen);

#endif