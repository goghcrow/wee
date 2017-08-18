#ifndef CHAN_H
#define CHAN_H

#include <stdbool.h>
#include "eventloop.h"
#include "closure.h"

struct chan
{
    int fd;
    struct eventloop *evloop;
    closure(on_write, void, void());
    closure(on_read, void, void());
    closure(on_close, void, void());
    closure(on_error, void, void());
    bool event_handling;
    bool in_loop;
};

struct chan *chan_create(struct eventloop *evloop, int fd)
{
}

void chan_release(struct chan *ch)
{
}
#endif