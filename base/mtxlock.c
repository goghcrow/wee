#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mtxlock.h"
#include "thread.h"

struct mtxlock
{
    pthread_mutex_t mtx;
    pthread_t thread;
};

struct mtxlock *mtl_create()
{
    struct mtxlock *lock = malloc(sizeof(*lock));
    assert(lock);
    memset(lock, 0, sizeof(*lock));
    RETCHECK(pthread_mutex_init(&lock->mtx, NULL));
    return lock;
}

void mtl_release(struct mtxlock *lock)
{
    assert(lock->thread == NULL);
    RETCHECK(pthread_mutex_destroy(&lock->mtx));
    memset(lock, 0, sizeof(*lock));
    free(lock);
}

void mtl_lock(struct mtxlock *lock)
{
    RETCHECK(pthread_mutex_lock(&lock->mtx));
    lock->thread = pthread_self();
}

void mtl_unlock(struct mtxlock *lock)
{
    // assert(lock->thread);
    lock->thread = NULL;
    RETCHECK(pthread_mutex_unlock(&lock->mtx));
}

bool mtl_lockedbyself(struct mtxlock *lock)
{
    return lock->thread && pthread_equal(lock->thread, pthread_self());
}

pthread_mutex_t *mtl_getmtx(struct mtxlock *lock)
{
    return &lock->mtx;
}