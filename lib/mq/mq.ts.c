#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "mq.h"

// -> head -> tail ->
struct mq
{
    int head;
    int tail;
    int cap;
    struct msg *q;
    pthread_mutex_t mutex;
    // struct mq *next;
};

static void *
safe_malloc(int n, char *file, unsigned long line)
{
    void *p = malloc(n);
    if (!p)
    {
        fprintf(stderr, "malloc %lu bytes fail in %s:%lu", (unsigned long)n, file, line);
    }
    memset(p, 0, n);
    return p;
}

static void mq_lock(struct mq *q)
{
    if (pthread_mutex_lock(&q->mutex))
    {
        abort();
    }
}

static void mq_unlock(struct mq *q)
{
    if (pthread_mutex_unlock(&q->mutex))
    {
        abort();
    }
}

#define SAFE_MALLOC(n) safe_malloc(n, __FILE__, __LINE__)
#define LOCK(q) mq_lock(q)
#define UNLOCK(q) mq_unlock(q)

static void
mq_expand(struct mq *q)
{
    struct msg *nq = SAFE_MALLOC(sizeof(*nq) * q->cap * 2);
    int i;
    assert(q->head == q->tail);

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

struct mq *
mq_create(int cap)
{
    assert(cap > 0);
    struct mq *q = SAFE_MALLOC(sizeof(*q));
    q->head = 0;
    q->tail = 0;
    q->cap = cap;
    q->q = SAFE_MALLOC(sizeof(struct msg) * cap);
    if (pthread_mutex_init(&q->mutex, 0))
    {
        abort();
    }
    return q;
}

void mq_release(struct mq *q)
{
    pthread_mutex_destroy(&q->mutex);
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

int mq_pop(struct mq *q, struct msg *msg)
{
    int ret = 1;

    LOCK(q);

    if (q->head != q->tail)
    {
        *msg = q->q[q->head++];
        ret = 0;

        if (q->head >= q->cap)
        {
            q->head = 0;
        }
    }

    UNLOCK(q);

    return ret;
}

void mq_dump(struct mq *q)
{
    int i, count;
    count = mq_count(q);

    printf("[ ");
    for (i = 0; i < count; i++)
    {
        if (i != 0)
        {
            printf(", ");
        }
        printf("%s", (char *)((q->q[(q->head + i) % q->cap]).ud));
        // printf("%p", (void *)&(q->q)[(q->head + i) % q->cap]);
    }
    printf("]\n");
}