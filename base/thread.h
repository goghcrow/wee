#ifndef THREAD_H
#define THREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

// https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
inline void thread_gettime(struct timespec *ts)
{
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;

#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}

// static inline int usleep(int64_t usec)
// {
//     struct timespec ts = {0, 0};
//     ts.tv_sec = usec / 1e6;
//     ts.tv_nsec = usec % 1000000 * 1000;
//     return nanosleep(&ts, NULL);
// }

#define MIN(a, b) ({        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
})

// https://stackoverflow.com/questions/558469/how-do-i-get-a-thread-id-from-an-arbitrary-pthread-t
inline uint64_t thread_getid()
{
    pthread_t t = pthread_self();
    uint64_t tid = 0;
    memcpy(&tid, &t, MIN(sizeof(t), sizeof(tid)));
    return tid;
}

// 检查 pthread 返回值
#define RETCHECK(ret) ({                                                            \
    int __err = (ret);                                                              \
    if (__builtin_expect(__err != 0, 0))                                            \
    {                                                                               \
        errno = __err;                                                              \
        perror("PTHREAD ERROR");                                                    \
        fprintf(stderr, "\tCaused by %s in %s:%d\n", __func__, __FILE__, __LINE__); \
        abort();                                                                    \
    }                                                                               \
})

#endif