#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "waitgroup.h"

struct waitgroup
{
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int count;
};

struct waitgroup *wg_create(int n)
{
    struct waitgroup *wg = malloc(sizeof(*wg));
    memset(wg, 0, sizeof(*wg));
    pthread_mutex_init(&wg->mtx, NULL);
    pthread_cond_init(&wg->cond, NULL);
    wg->count = n;
    return wg;
}

void wg_release(struct waitgroup *wg)
{
    pthread_mutex_destroy(&wg->mtx);
    pthread_cond_destroy(&wg->cond);
    memset(wg, 0, sizeof(*wg));
    free(wg);
}

void wg_done(struct waitgroup *wg)
{
    pthread_mutex_lock(&wg->mtx);
    if (--wg->count == 0)
    {
        pthread_cond_broadcast(&wg->cond);
    }
    pthread_mutex_unlock(&wg->mtx);
}

void wg_wait(struct waitgroup *wg)
{
    pthread_mutex_lock(&wg->mtx);
    while (wg->count > 0)
    {
        pthread_cond_wait(&wg->cond, &wg->mtx);
    }
    pthread_mutex_unlock(&wg->mtx);
}

int wg_count(struct waitgroup *wg)
{
    return wg->count;
}