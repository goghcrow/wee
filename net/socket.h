#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>

struct socket
{
    int fd;
};

int socket_client(char *host, char *port);
int socket_clientSync(char *host, char *port);
int socket_server(char *port);

void socket_setNonblock(int sockfd);
int socket_create();
bool socket_bind(int sockfd, const struct sockaddr *addr);
bool socket_listen(int sockfd);
int socket_accept(int sockfd, struct sockaddr *addr);
bool socket_connect(int sockfd, const struct sockaddr *addr);
ssize_t socket_read(int sockfd, void *buf, size_t count);
ssize_t socket_readv(int sockfd, const struct iovec *iov, int iovcnt);
ssize_t socket_write(int sockfd, const void *buf, size_t count);
bool socket_close(int sockfd);
bool socket_shutdownWrite(int sockfd);
int socket_getError(int sockfd);

#endif