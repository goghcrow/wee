#ifndef CHAN_H
#define CHAN_H

#include "eventloop.h"

// typedef void (*)(void) delegate;

struct chan
{
    int fd;
    void *onWiret;
    void *onReadl;
    void *onError;
};

struct chan *chan_create(struct eventloop *evloop, int fd)
{
}

void chan_release(struct chan *ch)
{
}
#endif