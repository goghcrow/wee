#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "sa.h"

/*
union sockaddr_all sa_create(uint16_t port, bool loopback_only);
union sockaddr_all sa_createV6(uint16_t port, bool loopback_only);
union sockaddr_all sa_fromip(const char *ip, uint16_t port);
union sockaddr_all sa_fromipV6(const char *ip, uint16_t port);

bool sa_resolve(char *hostname,  union sockaddr_all *u);
*/

void test_sa_create()
{
    char buf[SA_BUF_SIZE];
    union sockaddr_all u1 = sa_create(9999, true);
    union sockaddr_all u2 = sa_create(9999, false);

    sa_toip(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "127.0.0.1") == 0);

    sa_toipport(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "127.0.0.1:9999") == 0);

    sa_toip(&u2, buf, sizeof(buf));
    assert(strcmp(buf, "0.0.0.0") == 0);

    sa_toipport(&u2, buf, sizeof(buf));
    assert(strcmp(buf, "0.0.0.0:9999") == 0);
}

void test_sa_createV6()
{
    char buf[SA_BUF_SIZE];
    union sockaddr_all u1 = sa_createV6(9999, true);
    union sockaddr_all u2 = sa_createV6(9999, false);

    sa_toip(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "::1") == 0);

    sa_toipport(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "::1:9999") == 0);

    sa_toip(&u2, buf, sizeof(buf));
    assert(strcmp(buf, "::") == 0);

    sa_toipport(&u2, buf, sizeof(buf));
    assert(strcmp(buf, ":::9999") == 0);
}

void test_sa_fromip()
{
    char buf[SA_BUF_SIZE];
    union sockaddr_all u1 = sa_fromip("172.10.5.4", 8888);

    sa_toip(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "172.10.5.4") == 0);

    sa_toipport(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "172.10.5.4:8888") == 0);
}

void test_sa_fromipV6()
{
    char buf[SA_BUF_SIZE];
    union sockaddr_all u1 = sa_fromipV6("fe80::7a31:c1ff:fec2:32e6%en0", 8888);

    sa_toip(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "fe80:4::7a31:c1ff:fec2:32e6") == 0);

    sa_toipport(&u1, buf, sizeof(buf));
    assert(strcmp(buf, "fe80:4::7a31:c1ff:fec2:32e6:8888") == 0);
}

void test_sa_resolve()
{
    char buf[SA_BUF_SIZE];
    union sockaddr_all u1;
    memset(buf, 0, sizeof(buf));
    memset(&u1, 0, sizeof(u1));

    if (sa_resolve("www.google.com", &u1))
    {
        sa_toip(&u1, buf, sizeof(buf));
        // puts(buf);
    }
    else
    {
        assert(false);
    }
}

int main(void)
{
    test_sa_create();
    test_sa_createV6();

    test_sa_fromip();
    test_sa_fromipV6();

    test_sa_resolve();
    return 0;
}