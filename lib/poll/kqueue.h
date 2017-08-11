#ifndef KQUEUE_H
#define KQUEUE_H

#include <sys/event.h>
#include "socket_poll.h"

bool sp_invalid(int kfd)
{
    return kfd == -1;
}

int sp_create()
{
    return kqueue();
}

void sp_release(int kfd)
{
    close(kfd);
}

void sp_del(int kfd, int sock)
{
    struct kevent ke;
    EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(kfd, &ke, 1, NULL, 0, NULL);
    EV_SET(&ke, sock, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(kfd, &ke, 1, NULL, 0, NULL);
}

int sp_add(int kfd, int sock, void *ud)
{
    struct kevent ke;
    EV_SET(&ke, sock, EVFILT_READ, EV_ADD, 0, 0, ud);
    if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 || ke.flags & EV_ERROR)
    {
        return 1;
    }
    EV_SET(&ke, sock, EVFILT_WRITE, EV_ADD, 0, 0, ud);
    if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 || ke.flags & EV_ERROR)
    {
        EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        kevent(kfd, &ke, 1, NULL, 0, NULL);
        return 1;
    }
    EV_SET(&ke, sock, EVFILT_WRITE, EV_DISABLE, 0, 0, ud);
    if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 || ke.flags & EV_ERROR)
    {
        sp_del(kfd, sock);
        return 1;
    }
    return 0;
}

void sp_write(int kfd, int sock, void *ud, bool enable)
{
    struct kevent ke;
    EV_SET(&ke, sock, EVFILT_WRITE, enable ? EV_ENABLE : EV_DISABLE, 0, 0, ud);
    if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 || ke.flags & EV_ERROR)
    {
        // todo: check error
    }
}

int sp_wait(int kfd, struct event *e, int max)
{
    struct kevent ev[max];
    int n = kevent(kfd, NULL, 0, ev, max, NULL);

    int i;
    for (i = 0; i < n; i++)
    {
        e[i].s = ev[i].udata;
        unsigned filter = ev[i].filter;
        e[i].write = (filter == EVFILT_WRITE);
        e[i].read = (filter == EVFILT_READ);
        e[i].error = false; // kevent has not error event
    }

    return n;
}

#endif