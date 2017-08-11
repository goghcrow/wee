#ifndef EPOLL_H
#define EPOLL_H

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include "socket_poll.h"

bool sp_invalid(int efd)
{
    return efd == -1;
}

int sp_create()
{
    return epoll_create(1024);
}

void sp_release(int efd)
{
    close(efd);
}

int sp_add(int efd, int sock, void *ud)
{
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = ud;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev) == -1)
    {
        return 1;
    }
    return 0;
}

void sp_del(int efd, int sock)
{
    epoll_ctl(efd, EPOLL_CTL_DEL, sock, NULL);
}

void sp_write(int efd, int sock, void *ud, bool enable)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
    ev.data.ptr = ud;
    epoll_ctl(efd, EPOLL_CTL_MOD, sock, &ev);
}

int sp_wait(int efd, struct event *e, int max)
{
    struct epoll_event ev[max];
    int n = epoll_wait(efd, ev, max, -1);
    int i;
    for (i = 0; i < n; i++)
    {
        e[i].s = ev[i].data.ptr;
        unsigned flag = ev[i].events;
        e[i].write = (flag & EPOLLOUT) != 0;
        e[i].read = (flag & (EPOLLIN | EPOLLHUP)) != 0;
        e[i].error = (flag & EPOLLERR) != 0;
    }
    return n;
}

#endif