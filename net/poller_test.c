#include <stdio.h>
#include "poller.h"

int main(void)
{
    poll_fd fd = poller_create();
    printf("%d\n", fd);
    poller_release(fd);
    return 0;
}