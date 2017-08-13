#ifndef ADDR_H
#define ADDR_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef struct sockaddr SA;

const char *addr_satostr(struct sockaddr *sa)
{
    static char buf[INET_ADDRSTRLEN + 6];
    // inet_ntop(AF_INET, &((*((struct sockaddr_in*)&sa)).sin_addr.s_addr), buf, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, sa->sa_data + sizeof(in_port_t), buf, INET_ADDRSTRLEN);
    uint16_t port = ntohs(*(in_port_t *)sa->sa_data);
    sprintf(buf, "%s:%u", buf, port);
    return buf;
}

struct sockaddr addr_createByIpPort(char *ip, uint16_t port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    // 返回0地址无效, 返回-1错误
    assert(inet_pton(AF_INET, ip, &addr) > 0);
    // addr.sin_addr.s_addr = htonl(ip);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    return *(struct sockaddr *)(&addr);
}

struct sockaddr addr_createByPort(uint16_t port, bool loopback_only)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    // loopback 127.0.0.1      any 0.0.0.0
    in_addr_t ip = loopback_only ? INADDR_LOOPBACK : INADDR_ANY;
    addr.sin_addr.s_addr = htonl(ip);
    addr.sin_port = htons(port);
    return *(struct sockaddr *)(&addr);
}

bool addr_resolve(char *hostname, struct sockaddr *addr)
{
    assert(addr);
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, NULL, &hints, &res);
    if (status != 0)
    {
        fprintf(stderr, "ERROR getaddrinfo: %s\n", gai_strerror(status));
        return false;
    }
    // 只获取链表第一个元素
    // struct addrinfo *p; for(p = res; p != NULL; p = p->ai_next) { }
    if (res == NULL)
    {
        freeaddrinfo(res);
        false;
    }
    *addr = *(res->ai_addr);
    freeaddrinfo(res);
    return true;
}

#endif