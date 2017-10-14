#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "../base/waitgroup.h"
#include "socket.h"

#define BUFSZ 4096
static char buf[BUFSZ];

struct forward_args
{
    int src;
    int dst;
    struct waitgroup *wg;
};

static void forward_helper(int src, int dst, char* desc)
{
    ssize_t rcvd;
    size_t sent;

    while (1)
    {
        rcvd = recv(src, buf, BUFSZ, 0);
        if (rcvd == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("ERROR recv");
                socket_close(src);
                socket_close(dst);
                break;
            }
        }
        else if (rcvd == 0)
        {
            printf("%s closed\n", desc);
            // socket_shutdownWrite(dst);
            // fixme 何时关闭
            break;
        }
        else
        {
            buf[rcvd] = '\0';
            printf("%s recv %s\n", desc, buf);
            sent = socket_sendAllSync(dst, buf, rcvd);
            if (sent != rcvd)
            {
                socket_close(src);
                socket_close(dst);
                break;
            }
            printf("%s send success\n", desc);
        }
    }
}

static void *forward_thread1(void *ud)
{
    struct forward_args *args = (struct forward_args *)ud;
    forward_helper(args->src, args->dst, "threa1");
    wg_done(args->wg);
    return NULL;
}

static void *forward_thread2(void *ud)
{
    struct forward_args *args = (struct forward_args *)ud;
    forward_helper(args->dst, args->src, "thread2");
    wg_done(args->wg);
    return NULL;
}

static void forward(struct forward_args *args)
{
    pthread_t tid1, tid2;
    if (pthread_create(&tid1, NULL, forward_thread1, (void *)args))
    {
        abort();
    }
    if (pthread_create(&tid2, NULL, forward_thread2, (void *)args))
    {
        abort();
    }
}

int main(int argv, char **argc)
{
    struct forward_args args;
    
    args.src = socket_clientSync("127.0.0.1", "9000"); // nc -lk 9000
    args.dst = socket_clientSync("127.0.0.1", "9001"); // nc -lk 9001
    args.wg = wg_create(2);
    
    forward(&args);

    wg_wait(args.wg);
    return 0;
}