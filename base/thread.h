#ifndef THREAD_H
#define THREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define RETCHECK(ret) ({                                                          \
    int __err = (ret);                                                            \
    if (__builtin_expect(__err != 0, 0))                                          \
    {                                                                             \
        errno = __err;                                                            \
        perror("PTHREAD ERROR");                                                  \
        fprintf(stderr, "\tCaused by %s in %s:%d\n", __func__, __FILE__, __LINE__); \
        abort();                                                                  \
    }                                                                             \
})

#endif