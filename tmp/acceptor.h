// #ifndef ACCEPTOR_H
// #define ACCEPTOR_H

// #include <stdbool.h>
// #include <assert.h>
// #include <stdlib.h>
// #include <fcntl.h>
// #include <string.h>
// #include "poller.h"
// #include "socket.h"

// typedef void (*newConnCallbak)(int new_fd);

// struct acceptor
// {
//     int fd;
//     int idle_fd;
//     poll_fd poll_fd;
//     newConnCallbak onConnect;
//     bool listenning;
// };

// struct acceptor *acceptor_create(char *port,  poll_fd poll_fd)
// {
//     struct acceptor *acceptor = (struct acceptor *)malloc(sizeof(*acceptor));
//     assert(acceptor);
//     memset(acceptor, 0, sizeof(*acceptor));
//     acceptor->idle_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
//     assert(acceptor->idle_fd > 0);
//     acceptor->fd = socket_server(port);
// }

// bool acceptor_listen(struct acceptor *acceptor)
// {
//     if (acceptor->listenning || !socket_listen(acceptor->fd))
//     {
//         return false;
//     }

//     acceptor->listenning = true;

//     // return acceptor->poller->add(acceptor->poller->fd, int sock, void *ud));

//     int fd[2];
// }

// void acceptor_handleRead(struct acceptor *acceptor)
// {
//     struct sockaddr_storage *addr;
//     socket_accept(acceptor->fd, addr);
// }

// void acceptor_release(struct acceptor *acceptor)
// {
//     close(acceptor->fd);
//     free(acceptor);
// }

// #endif