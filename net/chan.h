#ifndef CHAN_H
#define CHAN_H

#include "eventloop.h"
#include <stdbool.h>



struct chan
{
    int fd;
    struct eventloop *evloop;
    delegate on_wiret;
    delegate on_read;
    delegate on_close;
    delegate on_error;
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