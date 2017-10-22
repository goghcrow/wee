#ifndef DUBBO_CLIENT_H
#define DUBBO_CLIENT_H

#include <stdbool.h>
#include <sys/time.h>

struct dubbo_args
{
    char *host;
    char *port;
    char *service;
    char *method;
    char *args;   /* JSON */
    char *attach; /* JSON */
    struct timeval timeout;
};

struct dubbo_client
{
    // struct buf;
    // callback ...
};

bool dubbo_invoke_sync(struct dubbo_args *);
// bool dubbo_invoke_async(struct dubbo_args *);

#endif