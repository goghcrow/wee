#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include "poller.h"

int poller_create()
{
    /* 1024 is just a hint for the kernel */
    int efd = epoll_create(1024);
    if (efd == -1)
    {
        perror("ERROR epoll_create");
    }
    return efd;
}

void poller_release(int efd)
{
    close(efd);
}

bool poller_add(int efd, int sock, void *ud)
{
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = ud;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev) == -1)
    {
        return false;
    }
    return true;
}

void poller_del(int efd, int sock)
{
    epoll_ctl(efd, EPOLL_CTL_DEL, sock, NULL);
}

void poller_write(int efd, int sock, void *ud, bool enable)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
    ev.data.ptr = ud;
    epoll_ctl(efd, EPOLL_CTL_MOD, sock, &ev);
}

int poller_wait(int efd, struct event *e, int max)
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