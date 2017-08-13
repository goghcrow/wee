#ifndef POLL_H
#define POLL_H

#include <stdbool.h>

typedef int poll_fd;

struct event
{
    void *s;
    bool read;
    bool write;
    bool error;
};

// -1 poller_invalid
poll_fd poller_create();
void poller_release(poll_fd fd);
bool poller_add(poll_fd fd, int sock, void *ud);
void poller_del(poll_fd fd, int sock);
void poller_write(poll_fd fd, int sock, void *ud, bool enable);
int poller_wait(poll_fd fd, struct event *e, int max);

#ifdef __APPLE__
#include "kqueue.h"
#else
#include "epoll.h"
#endif

#endif