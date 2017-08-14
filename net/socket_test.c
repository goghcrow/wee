#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "poller.h"
#include "socket.h"
#include "sa.h"

static char buf[1024];

// nc -kl 9999
void test_cli()
{
    poll_fd pfd = poller_create();
    int fd = socket_create();
    union sockaddr_all addr = sa_fromip("127.0.0.1", 9999);
    /*int i = */ socket_connect(fd, &addr, sizeof(addr.s));
    // if (i < 0)
    // {
    //     perror("ERROR connect");
    //     exit(1);
    // }
    poller_add(pfd, fd, NULL);

    // FIXME 客户端连接时候什么时候加入写事件
    poller_write(pfd, fd, NULL, true);

    struct event evts[1];
    memset(evts, 0, sizeof(evts));
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

// curl 127.0.0.1:9999
void test_serv()
{
    union sockaddr_all acceptSA;
    socklen_t addrlen = sizeof(acceptSA);

    int fd = socket_create();
    union sockaddr_all bindSA = sa_create(9999, true);
    
    socket_bind(fd, &bindSA, sizeof(bindSA.s));
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
                cfd = socket_accept(fd, &acceptSA, &addrlen);
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

void test_dns()
{
    union sockaddr_all u;
    
    char buf[INET6_ADDRSTRLEN];
    if (sa_resolve("www.youzan.com", &u)) {
        sa_toip(&u, buf, sizeof(buf));
        puts(buf);
    }
    else
    {
        printf("dns resolve fail\n");
    }
}

void test_sync()
{
    int sockfd = socket_clientSync("www.baidu.com", "80");
    char *payload = "GET / HTTP/1.1\r\n\r\n";
    char res[1024];
    write(sockfd, payload, strlen(payload));
    read(sockfd, res, sizeof(res));
    // send(sockfd, payload, strlen(payload), 0);
    // recv(sockfd, res, sizeof(res), 0);
    puts(res);
    close(sockfd);
}

void test_sync_serv()
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    int listen_fd = socket_clientSync(NULL, "9999");
    if (socket_listen(listen_fd) == false)
    {
        return;
    }

    int new_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd < 0)
    {
        perror("ERROR accept");
        return;
    }
    close(listen_fd);

    char *payload = "HELLO\r\n";
    char res[1024];
    write(new_fd, payload, strlen(payload));
    // socket_shutdownWrite(new_fd);
    read(new_fd, res, sizeof(res));
    close(new_fd);
    puts(res);
}

int main(void)
{
    // test_cli();
    // test_serv();
    // test_dns();
    // test_sync();
    // test_sync_serv();

    return 0;
}