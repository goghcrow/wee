#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "waitgroup.h"

void *work(void *ud)
{
    struct waitgroup *wg = (struct waitgroup *)ud;
    puts("working...");
    sleep(1);
    wg_done(wg);
    puts("work done");
    return NULL;
}

#define N_WORKER 3

int main(int argc, char **argv)
{
    int i;
    pthread_t workers[N_WORKER];
    struct waitgroup *wg = wg_create(N_WORKER);

    for (i = 0; i < N_WORKER; i++)
    {
        pthread_create(&workers[i], NULL, work, (void *)wg);
    }

    puts("waiting...");
    wg_wait(wg);
    wg_release(wg);
    puts("all done");
    return 0;
}