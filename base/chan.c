#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "mtxlock.h"
#include "cond.h"
#include "mq.h"
#include "chan.h"

struct chan
{
    struct mtxlock *lock;
    struct cond *rcv_cond;
    struct cond *snd_cond;
    struct mq *q;
    int cap;
};

struct chan *ch_create(int cap)
{
    assert(cap >= 0);
    struct chan *ch = malloc(sizeof(*ch));
    assert(ch);
    memset(ch, 0, sizeof(*ch));
    ch->lock = mtl_create();
    ch->rcv_cond = cond_create(ch->lock);
    ch->snd_cond = cond_create(ch->lock);
    ch->q = mq_create(cap + 1); // mq 满自动扩容
    ch->cap = cap;
    return ch;
}

void ch_release(struct chan *ch)
{
    mtl_release(ch->lock);
    cond_release(ch->rcv_cond);
    cond_release(ch->snd_cond);
    mq_release(ch->q);
    free(ch);
}

void ch_send(struct chan *ch, struct msg *msg)
{
    mtl_lock(ch->lock);
    if (ch->cap > 0)
    {
        while (mq_count(ch->q) >= ch->cap)
        {
            cond_wait(ch->snd_cond);
        }
        assert(mq_count(ch->q) < ch->cap);
    }
    mq_push(ch->q, msg);
    cond_signal(ch->rcv_cond);
    mtl_unlock(ch->lock);
}

bool ch_recv(struct chan *ch, struct msg *msg)
{
    bool r;
    mtl_lock(ch->lock);
    while (!mq_count(ch->q))
    {
        cond_wait(ch->rcv_cond);
    }
    r = mq_pop(ch->q, msg);
    if (ch->cap > 0)
    {
        cond_signal(ch->snd_cond);
    }
    mtl_unlock(ch->lock);
    return r;
}