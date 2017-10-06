#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "thread.h"
#include "mq.h"

// head -> tail
struct mq
{
    int head;
    int tail;
    int cap;
    struct msg *q;
/* struct mq *next; */
#ifdef MQ_THREAD_SAFE
    pthread_mutex_t mutex;
#endif
};

/* cc -DMQ_THREAD_SAFE */
#ifdef MQ_THREAD_SAFE
#define LOCK(q) RETCHECK(pthread_mutex_lock(&q->mutex))
#define UNLOCK(q) RETCHECK(pthread_mutex_unlock(&q->mutex))
#else
#define LOCK(q)
#define UNLOCK(q)
#endif

static void mq_expand(struct mq *q)
{
    assert(q->head == q->tail);

    struct msg *nq = calloc(q->cap * 2, sizeof(*nq));
    assert(nq);

    int i;
    for (i = 0; i < q->cap; i++)
    {
        nq[i] = q->q[(q->head + i) % q->cap];
    }

    q->head = 0;
    q->tail = q->cap;
    q->cap *= 2;
    free(q->q);
    q->q = nq;
}

struct mq *mq_create(int cap)
{
    assert(cap > 0);
    struct mq *q = malloc(sizeof(*q));
    assert(q);
    memset(q, 0, sizeof(*q));
    q->head = 0;
    q->tail = 0;
    q->cap = cap;
    q->q = calloc(cap, sizeof(struct msg));
    assert(q->q);
#ifdef MQ_THREAD_SAFE
    RETCHECK(pthread_mutex_init(&q->mutex, NULL));
#endif
    return q;
}

void mq_release(struct mq *q)
{
#ifdef MQ_THREAD_SAFE
    RETCHECK(pthread_mutex_destroy(&q->mutex));
#endif
    free(q->q);
    free(q);
}

int mq_count(struct mq *q)
{
    LOCK(q);
    int head = q->head;
    int tail = q->tail;
    int cap = q->cap;
    UNLOCK(q);

    if (head <= tail)
    {
        return tail - head;
    }
    else
    {
        return tail + cap - head;
    }
}

void mq_push(struct mq *q, struct msg *msg)
{
    assert(msg);

    LOCK(q);

    q->q[q->tail] = *msg;

    if (++q->tail >= q->cap)
    {
        q->tail = 0;
    }

    if (q->head == q->tail)
    {
        mq_expand(q);
    }

    UNLOCK(q);
}

bool mq_pop(struct mq *q, struct msg *msg)
{
    bool ret = false;

    LOCK(q);

    if (q->head != q->tail)
    {
        *msg = q->q[q->head++];
        ret = true;

        if (q->head >= q->cap)
        {
            q->head = 0;
        }
    }

    UNLOCK(q);

    return ret;
}