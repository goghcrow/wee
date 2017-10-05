#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "mtxlock.h"
#include "thread.h"

void *fun1(void *ud)
{
    struct mtxlock *lock = (struct mtxlock *)ud;
    puts("func1 locking");
    mtl_lock(lock);
    puts("func1 locked");
    sleep(1);
    puts("func1 unlocking");
    mtl_unlock(lock);
    return NULL;
}

void *fun2(void *ud)
{
    struct mtxlock *lock = (struct mtxlock *)ud;
    puts("func2 locking");
    mtl_lock(lock);
    puts("func2 locked");
    sleep(1);
    puts("func2 unlocking");
    mtl_unlock(lock);
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;
    struct mtxlock *lock = mtl_create();
    RETCHECK(pthread_create(&t1, NULL, fun1, (void *)lock));
    RETCHECK(pthread_create(&t2, NULL, fun2, (void *)lock));
    RETCHECK(pthread_join(t1, NULL));
    RETCHECK(pthread_join(t2, NULL));
    mtl_release(lock);
    return 0;
}