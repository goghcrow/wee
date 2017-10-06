#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "mtxlock.h"
#include "cond.h"
#include "mq.h"
#include "chan.h"

#ifndef INIT_CHAN_CAP
#define INIT_CHAN_CAP 8
#endif

struct chan
{
    struct mtxlock *lock;
    struct cond *cond;
    struct mq *q;
};

struct chan *ch_create()
{
    struct chan *ch = malloc(sizeof(*ch));
    assert(ch);
    memset(ch, 0, sizeof(*ch));
    ch->lock = mtl_create();
    ch->cond = cond_create(ch->lock);
    ch->q = mq_create(INIT_CHAN_CAP);
    return ch;
}

void ch_release(struct chan *ch)
{
    mtl_release(ch->lock);
    cond_release(ch->cond);
    mq_release(ch->q);
    free(ch);
}

void ch_send(struct chan *ch, struct msg *msg)
{
    mtl_lock(ch->lock);
    mq_push(ch->q, msg);
    cond_signal(ch->cond);
    mtl_unlock(ch->lock);
}

bool ch_recv(struct chan *ch, struct msg *msg)
{
    bool r;
    mtl_lock(ch->lock);
    while (!mq_count(ch->q))
    {
        cond_wait(ch->cond);
    }
    r = mq_pop(ch->q, msg);
    mtl_unlock(ch->lock);
    return r;
}