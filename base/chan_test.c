#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "chan.h"
#include "thread.h"


void *consumer(void *ud)
{
    struct chan *ch = (struct chan *)ud;
    struct msg msg;
    int tid = thread_getid();

    while (1)
    {
        if (ch_recv(ch, &msg)) {
            printf("tid=%d consumer %s\n", tid, msg.ud);
            free(msg.ud);
        }
    }
    return NULL;
}

void *producer(void *ud)
{
    struct chan *ch = (struct chan *)ud;
    struct msg msg;
    int i = 0;
    char buf[100];
    while (1)
    {
        snprintf(buf, 100, "msg %d", ++i);
        msg.ud = strdup(buf);
        msg.sz = strlen(buf);
        puts("producer send");
        ch_send(ch, &msg);
        usleep(100 * 1000);
    }
    return NULL;
}

#define N_CONSUMER 10
#define N_PRODUCER 1

int main(void)
{
    int i;
    pthread_t consumers[N_CONSUMER];
    pthread_t producers[N_PRODUCER];

    struct chan *ch = ch_create();

    for (i = 0; i < N_CONSUMER; i++)
    {
        pthread_create(&consumers[i], NULL, consumer, (void *)ch);
    }
    for (i = 0; i < N_PRODUCER; i++)
    {
        pthread_create(&producers[i], NULL, producer, (void *)ch);
    }

    for (i = 0; i < N_CONSUMER; i++)
    {
        pthread_join(consumers[i], NULL);
    }
    for (i = 0; i < N_PRODUCER; i++)
    {
        pthread_join(producers[i], NULL);
    }

    return 0;
}