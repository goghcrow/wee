#include <stdio.h>
#include "socket_poll.h"

int main(void)
{
    poll_fd fd = sp_create();
    printf("%d\n", fd);
    sp_release(fd);
    return 0;
}