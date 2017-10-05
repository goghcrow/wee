#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cond.h"
#include "mtxlock.h"
#include "waiter.h"

struct waiter
{
    struct mtxlock *lock;
    struct cond *cond;
    int signaled;
};

struct waiter *waiter_create()
{
    struct waiter *w = malloc(sizeof(*w));
    assert(w);
    memset(w, 0, sizeof(*w));
    w->lock = mtl_create();
    w->cond = cond_create(w->lock);
    return w;
}

void waiter_release(struct waiter *w)
{
    mtl_release(w->lock);
    cond_release(w->cond);
    memset(w, 0, sizeof(*w));
    free(w);
}

void waiter_signal(struct waiter *w)
{
    mtl_lock(w->lock);
    w->signaled = 1;
    cond_broadcast(w->cond);
    mtl_unlock(w->lock);
}

void waiter_wait(struct waiter *w)
{
    while (!w->signaled)
    {
        cond_wait(w->cond);
    }
}
