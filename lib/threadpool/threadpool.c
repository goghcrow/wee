#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "threadpool.h"
#include "queue.h"

#define MAX_THREADPOOL_SIZE 128

struct threadpool
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;

    unsigned int idle_threads;
    unsigned int nthreads;
    pthread_t *threads;

    QUEUE wq;
    QUEUE exit_message;

    volatile int initialized;
};

struct threadpool_task
{
    void (*work)(struct threadpool_task *task, void *arg);
    void *arg;
    QUEUE wq;
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
#define SAFE_MALLOC(n) safe_malloc(n, __FILE__, __LINE__)

static void
thread_create(pthread_t *tid, void (*entry)(void *arg), void *arg)
{
    if (pthread_create(tid, NULL, (void *(*)(void *))entry, arg))
    {
        abort();
    }
}

static void
thread_join(pthread_t *tid)
{
    if (pthread_join(*tid, NULL))
    {
        abort();
    }
}

static void
mutex_init(pthread_mutex_t *mutex)
{
    if (pthread_mutex_init(mutex, NULL))
    {
        abort();
    }
}

static void
mutex_lock(pthread_mutex_t *mutex)
{
    if (pthread_mutex_lock(mutex))
    {
        abort();
    }
}

static void
mutex_unlock(pthread_mutex_t *mutex)
{
    if (pthread_mutex_unlock(mutex))
    {
        abort();
    }
}

static void
mutex_destroy(pthread_mutex_t *mutex)
{
    if (pthread_mutex_destroy(mutex))
    {
        abort();
    }
}

static void
cond_init(pthread_cond_t *cond)
{
    if (pthread_cond_init(cond, NULL))
    {
        abort();
    }
}

static void
cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    if (pthread_cond_wait(cond, mutex))
    {
        abort();
    }
}

static void
cond_signal(pthread_cond_t *cond)
{
    if (pthread_cond_signal(cond))
    {
        abort();
    }
}

static void
cond_destroy(pthread_cond_t *cond)
{
    if (pthread_cond_destroy(cond))
    {
        abort();
    }
}

static void
worker(void *arg)
{
    struct threadpool *pool;
    struct threadpool_task *task;
    QUEUE *q;

    pool = (struct threadpool *)arg;

    while (1)
    {
        mutex_lock(&pool->mutex);
        while (QUEUE_EMPTY(&pool->wq))
        {
            pool->idle_threads++;
            cond_wait(&pool->cond, &pool->mutex);
            pool->idle_threads--;
        }

        q = QUEUE_HEAD(&pool->wq);
        if (q == &pool->exit_message)
        {
            // exit_message 不移除q, 线程池逐一停止work
            cond_signal(&pool->cond);
        }
        else
        {
            QUEUE_REMOVE(q);
            QUEUE_INIT(q);
        }
        mutex_unlock(&pool->mutex);

        if (q == &pool->exit_message)
        {
            break;
        }

        task = QUEUE_DATA(q, struct threadpool_task, wq);
        task->work(task, task->arg);
        task->work = NULL; /* Signal cancel() that the work req is done executing. */
    }
}

static void post(struct threadpool *pool, QUEUE *q)
{
    mutex_lock(&pool->mutex);
    QUEUE_INSERT_TAIL(&pool->wq, q);
    if (pool->idle_threads > 0)
    {
        cond_signal(&pool->cond);
    }
    mutex_unlock(&pool->mutex);
}

struct threadpool *
threadpool_create(int size)
{
    int i;
    struct threadpool *pool = SAFE_MALLOC(sizeof(*pool));

    if (size < 1)
    {
        size = 1;
    }
    if (size > MAX_THREADPOOL_SIZE)
    {
        size = MAX_THREADPOOL_SIZE;
    }
    pool->nthreads = size;
    pool->threads = SAFE_MALLOC(size * sizeof(pthread_t));

    cond_init(&pool->cond);
    mutex_init(&pool->mutex);

    QUEUE_INIT(&pool->wq);

    for (i = 0; i < size; i++)
    {
        thread_create(pool->threads + i, worker, pool);
    }

    pool->initialized = 1;

    return pool;
}

void threadpool_release(struct threadpool *pool)
{
    int i;
    if (pool->initialized == 0)
    {
        return;
    }

    post(pool, &pool->exit_message);

    for (i = 0; i < pool->nthreads; i++)
    {
        thread_join(pool->threads + i);
    }
    free(pool->threads);

    mutex_destroy(&pool->mutex);
    cond_destroy(&pool->cond);

    free(pool);
}

void threadpool_submit(struct threadpool *pool, struct threadpool_task *task)
{
    assert(task->work);
    post(pool, &task->wq);
}

int threadpool_cancel(struct threadpool *pool, struct threadpool_task *task)
{
    int cancelled;

    mutex_lock(&pool->mutex);
    cancelled = !QUEUE_EMPTY(&pool->wq) && task->work != NULL;
    if (cancelled)
    {
        QUEUE_REMOVE(&task->wq);
    }
    mutex_unlock(&pool->mutex);

    return cancelled;
}

struct threadpool_task *threadpool_task_create(void (*work)(struct threadpool_task *task, void *arg), void *arg)
{
    struct threadpool_task *task = SAFE_MALLOC(sizeof(*task));
    task->work = work;
    task->arg = arg;
    return task;
}

void threadpool_task_release(struct threadpool_task *task)
{
    free(task);
}