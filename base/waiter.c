#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "waiter.h"

struct waiter
{
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int signaled;
};

struct waiter *waiter_create()
{
    struct waiter *w = malloc(sizeof(*w));
    memset(w, 0, sizeof(*w));
    pthread_mutex_init(&w->mtx, NULL);
    pthread_cond_init(&w->cond, NULL);
    return w;
}

void waiter_release(struct waiter *w)
{
    pthread_mutex_destroy(&w->mtx);
    pthread_cond_destroy(&w->cond);
    memset(w, 0, sizeof(*w));
    free(w);
}

void waiter_signal(struct waiter *w)
{
    pthread_mutex_lock(&w->mtx);
    w->signaled = 1;
    pthread_cond_broadcast(&w->cond);
    pthread_mutex_unlock(&w->mtx);
}

void waiter_wait(struct waiter *w)
{
    pthread_mutex_lock(&w->mtx);
    while (!w->signaled)
    {
        pthread_cond_wait(&w->cond, &w->mtx);
    }
    pthread_mutex_unlock(&w->mtx);
}
