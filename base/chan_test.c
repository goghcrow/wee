#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "chan.h"
#include "thread.h"

void *consumer(void *ud)
{
    int tid = thread_getid();

    struct chan *ch = (struct chan *)ud;
    struct msg msg;

    while (1)
    {
        printf("tid=%d consuming...\n", tid);
        if (ch_recv(ch, &msg))
        {
            printf("[OK]tid=%d consumed %s\n", tid, msg.ud);
            free(msg.ud);
        }
    }
    return NULL;
}

void *producer(void *ud)
{
    int tid = thread_getid();

    struct chan *ch = (struct chan *)ud;
    struct msg msg;
    int i = 0;
    char buf[100];
    while (1)
    {
        printf("tid=%d producing...\n", tid);
        snprintf(buf, 100, "msg %d", ++i);
        msg.ud = strdup(buf);
        msg.sz = strlen(buf);
        ch_send(ch, &msg);
        printf("[OK]tid=%d produced..\n", tid);
        usleep(500 * 1000);
    }
    return NULL;
}
void test_chan(int CHAN_CAP, int N_PRODUCER, int N_CONSUMER)
{
    int i;
    pthread_t consumers[N_CONSUMER];
    pthread_t producers[N_PRODUCER];

    struct chan *ch = ch_create(CHAN_CAP);

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
}



int main(void)
{
    // test_chan(0, 1, 10);
    // test_chan(1, 1, 0);
    // test_chan(1, 0, 2);
    test_chan(2, 2, 1);
    return 0;
}