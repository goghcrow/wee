#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "waiter.h"

void *func_wait(void *ud)
{
    struct waiter *w = (struct waiter *)ud;
    puts("waiting...");
    waiter_wait(w);
    puts("waited");
    return NULL;
}

void *func_signal(void *ud)
{
    struct waiter *w = (struct waiter *)ud;
    sleep(1);
    waiter_signal(w);
    puts("signal");
    return NULL;
}

#define N_WAIT 5

int main(void)
{
    int i;
    pthread_t wait_threads[N_WAIT];
    pthread_t signal_thread;

    struct waiter *w = waiter_create();

    for (i = 0; i < N_WAIT; i++)
    {
        pthread_create(&wait_threads[i], NULL, func_wait, (void *)w);
    }
    pthread_create(&signal_thread, NULL, func_signal, (void *)w);

    for (i = 0; i < N_WAIT; i++)
    {
        pthread_join(wait_threads[i], NULL);
    }
    pthread_join(signal_thread, NULL);

    waiter_release(w);

    puts("done");

    // // 如果使用pthread_exit 等待所有进程结束
    // pthread_exit(NULL);
    // // 以下并不会执行 !!!!!
    // waiter_release(w);
    // puts("return");
    return 0;
}