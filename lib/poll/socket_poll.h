#ifndef POLL_H
#define POLL_H

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

typedef int poll_fd;

struct event
{
    void *s;
    bool read;
    bool write;
    bool error;
};

bool sp_invalid(poll_fd fd);
poll_fd sp_create();
void sp_release(poll_fd fd);
int sp_add(poll_fd fd, int sock, void *ud);
void sp_del(poll_fd fd, int sock);
void sp_write(poll_fd fd, int sock, void *ud, bool enable);
int sp_wait(poll_fd fd, struct event *e, int max);

void sp_nonblocking(int sock)
{
    int flag = fcntl(sock, F_GETFL, 0);
    if (flag == -1)
    {
        return;
    }
    fcntl(sock, F_SETFL, flag | O_NONBLOCK);
}

#ifdef __linux__
#include "epoll.h"
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include "kqueue.h"
#endif

#endif