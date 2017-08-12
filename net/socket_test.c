#include <stdio.h>
#include "socket.h"
#include "poller/poller.h"
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>

static char buf[1024];

// curl 127.0.0.1:9999
void test_serv()
{
    SA acceptSA;
    int fd = socket_create();
    SA addr = socket_createInetAddrPort(9999, true);
    socket_bind(fd, &addr);
    socket_listen(fd);

    poll_fd pfd = poller_create();
    poller_add(pfd, fd, NULL);

    struct event evts[1];
    struct event evt;
    int cfd = 0;

    while (true)
    {
        int n = poller_wait(pfd, evts, sizeof(evts));
        if (n <= 0)
        {
            n = 0;
            if (errno == EINTR)
            {
                continue;
            }
        }

        evt = evts[0];
        if (evt.read)
        {
            if (cfd)
            {
                ssize_t rn = socket_read(cfd, buf, sizeof(buf));
                if (rn)
                {
                    buf[rn] = '\0';
                    puts(buf);
                    poller_write(pfd, cfd, NULL, true);
                }
                else
                {
                    puts("closed");
                    goto clear;
                }
            }
            else
            {
                cfd = socket_accept(fd, &acceptSA);
                poller_add(pfd, cfd, NULL);
            }
        }
        else if (evt.write)
        {
            puts("write");
            poller_write(pfd, cfd, NULL, false);
            char resp[] = "HTTP/1.0 200 OK\r\n\r\nHELLO";
            socket_write(cfd, resp, sizeof(resp));
            goto clear;
        }
        if (evt.error)
        {
            perror("ERROR poller_wait error");
            goto clear;
        }
    }

clear:
    poller_del(pfd, cfd);
    socket_close(cfd);
    poller_release(pfd);
    socket_close(fd);
    return;
}

// nc -kl 9999
void test_cli()
{
    poll_fd pfd = poller_create();
    int fd = socket_create();
    SA addr = socket_createInetAddrIpPort("127.0.0.1", 9999);
    /*int i = */ socket_connect(fd, &addr);
    // if (i < 0)
    // {
    //     perror("ERROR connect");
    //     exit(1);
    // }
    poller_add(pfd, fd, NULL);

    // FIXME 客户端连接时候什么时候加入写事件
    poller_write(pfd, fd, NULL, true);

    struct event evts[1];
    bzero(evts, sizeof(evts));
    int n = 0;
    int i = 0;

    while (true)
    {
        n = poller_wait(pfd, evts, sizeof(evts));
        if (n <= 0)
        {
            n = 0;
            if (errno == EINTR)
            {
                continue;
            }
        }

        for (i = 0; i < n; i++)
        {
            if (evts[0].read)
            {
                // connected
                ssize_t rn = socket_read(fd, buf, sizeof(buf));
                if (rn < 0)
                {
                    perror("ERROR read");
                    goto clear;
                }
                else if (rn == 0)
                {
                    puts("close");
                    goto clear;
                }
            }
            else if (evts[0].write)
            {
                printf("write\n");
                poller_write(pfd, fd, NULL, false);
                int w = socket_write(fd, "HELLO\n", 6);
                if (w < 0)
                {
                    perror("ERROR write");
                }
                goto clear;
            }
        }
    }

    int errno_ = socket_getError(fd);
    assert(errno_ == 0);

clear:
    poller_del(pfd, fd);
    socket_close(fd);
    poller_release(pfd);
}

int main(void)
{
    test_cli();
    // test_serv();
    return 0;
}