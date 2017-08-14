#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include "socket.h"
#include "sa.h"

void sigchld_handler(int s)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

bool register_sig_handler()
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("ERROR sigaction SIGCHLD");
        return false;
    }

    return true;
}

void server()
{
    int sockfd = socket_serverSync("9999");
    if (sockfd < 0)
    {
        return;
    }
    if (!register_sig_handler())
    {
        return;
    }

    printf("server starting...\n");

    if (!socket_listen(sockfd))
    {
        return;
    }

    int new_fd;
    union sockaddr_all addr;
    socklen_t addrlen = sizeof(addr);
    char buf[INET6_ADDRSTRLEN];

    while (true)
    {
        new_fd = socket_acceptSync(sockfd, &addr, &addrlen);
        if (new_fd < 0)
        {
            continue;
        }

        sa_toipport(&addr, buf, sizeof(buf));
        printf("accept connection from %s\n", buf);

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("ERROR fork");
            return;
        }

        if (pid == 0)
        {
            // SOCK_CLOEXEC 不需要主动关闭
            socket_close(sockfd);
            close(sockfd);
            socket_write(new_fd, "HELLO\n", 6);
            socket_close(new_fd);
            exit(0);
        }

        socket_close(new_fd);
    }
}

int main(void)
{
    server();
    return 0;
}