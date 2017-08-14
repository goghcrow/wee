#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct
{
} * Object;

Object new ()
{
    Object obj = malloc(sizeof(*obj));
    return obj;
}

void ReleaseObject(Object obj)
{
    free(obj);
}

int main(int argc, const char *argv[])
{
    Object obj = new ();

    printf("sizeof sockaddr is %lu\n", sizeof(struct sockaddr));
    printf("sizeof sockaddr_in is %lu\n", sizeof(struct sockaddr_in));
    printf("sizeof sockaddr_in6 is %lu\n", sizeof(struct sockaddr_in6));
    printf("sizeof sockaddr_all is %lu\n", sizeof(union sockaddr_all {
        struct sockaddr s;
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    }));
    printf("sizeof sockaddr_storage is %lu\n", sizeof(struct sockaddr_storage));
    
    return 0;
}