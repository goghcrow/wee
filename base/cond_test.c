#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "mtxlock.h"
#include "cond.h"

static void *fn_wait(void *ud)
{
    struct cond *c = (struct cond *)ud;
    puts("waiting signal...");

    struct mtxlock *lock = cond_getlock(c);
    // fixme while() {}
    mtl_lock(lock);
    cond_wait(c);
    mtl_unlock(lock);
    puts("recved signal");
    puts("====================");
    return NULL;
}

static void *fn_timedwait(void *ud)
{
    struct cond *c = (struct cond *)ud;
    puts("waiting signal...");

    struct mtxlock *lock = cond_getlock(c);
    // fixme while() {}
    mtl_lock(lock);
    if (cond_timedwait(c, 0.5)) {
        puts("timeout");
    } else {
        assert(false);
    }
    mtl_unlock(lock);
    puts("recved signal");
    return NULL;
}

static void *fn_signal(void *ud)
{
    struct cond *c = (struct cond *)ud;
    puts("waiting 1 sec");
    sleep(1);
    puts("signal");
    cond_signal(c);
    return NULL;
}

static void *fn_broadcast(void *ud)
{
    struct cond *c = (struct cond *)ud;
    puts("waiting 1 sec");
    sleep(1);
    puts("broadcast");
    cond_broadcast(c);
    return NULL;
}

static void test_signal()
{
    pthread_t t1;
    pthread_t t2;

    struct mtxlock *l = mtl_create();
    struct cond *c = cond_create(l);

    pthread_create(&t1, NULL, fn_wait, (void *)c);
    pthread_create(&t2, NULL, fn_signal, (void *)c);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    cond_release(c);
    mtl_release(l);
}

static void test_broadcast()
{
    pthread_t t_wait1;
    pthread_t t_wait2;
    pthread_t t_signal;

    struct mtxlock *l = mtl_create();
    struct cond *c = cond_create(l);

    pthread_create(&t_wait1, NULL, fn_wait, (void *)c);
    pthread_create(&t_wait2, NULL, fn_wait, (void *)c);
    pthread_create(&t_signal, NULL, fn_broadcast, (void *)c);

    pthread_join(t_wait1, NULL);
    pthread_join(t_wait2, NULL);
    pthread_join(t_signal, NULL);

    cond_release(c);
    mtl_release(l);
}

void test_cond_wait()
{
    pthread_t t1;
    pthread_t t2;

    struct mtxlock *l = mtl_create();
    struct cond *c = cond_create(l);

    pthread_create(&t1, NULL, fn_timedwait, (void *)c);
    pthread_create(&t2, NULL, fn_signal, (void *)c);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    cond_release(c);
    mtl_release(l);
}

int main(void)
{
    test_signal();
    test_broadcast();
    test_cond_wait();
    return 0;
}