#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "eventloop.h"
#include "poller.h"

struct eventloop
{
    int poll_fd;
    int wakeup_fd; // FIX eventFd
    bool looping;
    bool quit;
    bool event_handling;
};

struct eventloop *evloop_create()
{
    struct eventloop *evloop = malloc(sizeof(*evloop));
    assert(evloop);
    memset(evloop, 0, sizeof(*evloop));
    evloop->poll_fd = poller_create();
    assert(evloop->poll_fd != -1);
    evloop->looping = false;
    evloop->quit = false;
    evloop->event_handling = false;
    return evloop;
}

void evloop_release(struct eventloop *evloop)
{
    poller_release(evloop->poll_fd);
    free(evloop);
}

void evloop_quit(struct eventloop *evloop)
{
    evloop->quit = true;
}

void evloop_loop(struct eventloop *evloop)
{
    assert(!evloop->looping);
    evloop->looping = true;
    evloop->quit = false;

    int max = 100;
    struct event e[max];
    while (!evloop->quit)
    {
        poller_wait(evloop->poll_fd, e, sizeof(e));
        evloop->event_handling = true;
        evloop->event_handling = false;
    }
    evloop->quit = false;
}

bool poller_add(poll_fd fd, int sock, void *ud)
{
}
void poller_del(poll_fd fd, int sock)
{
}
void poller_write(poll_fd fd, int sock, void *ud, bool enable)
{
}

void