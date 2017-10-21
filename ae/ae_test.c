#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "anet.h"
#include "ae.h"

char err[ANET_ERR_LEN];

int after_test(struct aeEventLoop *el, long long id, void *ud)
{
    long long tickId = (long long)ud;
    aeDeleteTimeEvent(el, tickId);

    aeStop(el);
    return AE_NOMORE;
}

int tick_test(struct aeEventLoop *el, long long id, void *ud)
{
    puts("tick");
    return 1000;
}

int main(void)
{
    struct aeEventLoop *el = aeCreateEventLoop(1024);
    if (el == NULL)
    {
        fprintf(stderr, "Failed creating the event loop. Error message: '%s'", strerror(errno));
        exit(1);
    }

    long long tickId = aeCreateTimeEvent(el, 1000, tick_test, NULL, NULL);
    aeCreateTimeEvent(el, 5000, after_test, (void *)tickId, NULL);

    aeMain(el);
    aeDeleteEventLoop(el);

    return 0;
}