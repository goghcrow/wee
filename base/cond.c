#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cond.h"
#include "thread.h"
#include "mtxlock.h"

// signal 表示资源就绪
// broadcast 表示事件发生, 状态改变

struct cond
{
    struct mtxlock *lock;
    pthread_cond_t cond;
};

struct cond *cond_create(struct mtxlock *lock)
{
    struct cond *c = malloc(sizeof(*c));
    assert(c);
    memset(c, 0, sizeof(*c));
    c->lock = mtl_create();
    RETCHECK(pthread_cond_init(&c->cond, NULL));
    return c;
}

void cond_release(struct cond *c)
{
    // 注意: 不负责释放 mtl_release(&c->lock);
    RETCHECK(pthread_cond_destroy(&c->cond));
    memset(c, 0, sizeof(*c));
    free(c);
}

void cond_signal(struct cond *c)
{
    RETCHECK(pthread_cond_signal(&c->cond));
}

void cond_broadcast(struct cond *c)
{
    RETCHECK(pthread_cond_broadcast(&c->cond));
}

void cond_wait(struct cond *c)
{
    mtl_lock(c->lock);
    pthread_mutex_t *mtx = mtl_getmtx(c->lock);
    RETCHECK(pthread_cond_wait(&c->cond, mtx));
    mtl_unlock(c->lock);
}

// fixme
// bool cond_timedwait(struct cond *c)
// {
//     bool r = false;
//     mtl_lock(c->lock);
//     pthread_mutex_t *mtx = mtl_getmtx(c->lock);
//     r = ETIMEDOUT == pthread_cond_timedwait(&c->cond, mtx);
//     mtl_unlock(c->lock);
//     return r;
// }
