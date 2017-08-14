## 

使用 union sockaddr_all 而不使用 sockaddr_storage

是因为sockaddr_storage占空间太大, 且我们只需要ipv4与ipv6, 不需要其他支持

```c
printf("sizeof sockaddr is %lu\n", sizeof(struct sockaddr));
printf("sizeof sockaddr_in is %lu\n", sizeof(struct sockaddr_in));
printf("sizeof sockaddr_in6 is %lu\n", sizeof(struct sockaddr_in6));
printf("sizeof sockaddr_all is %lu\n", sizeof(union sockaddr_all {
    struct sockaddr s;
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;
}));
printf("sizeof sockaddr_storage is %lu\n", sizeof(struct sockaddr_storage));

sizeof sockaddr is 16
sizeof sockaddr_in is 16
sizeof sockaddr_in6 is 28
sizeof sockaddr_all is 28
sizeof sockaddr_storage is 128

/* 
accept(int sockfd, union sockaddr_all *addr, socklen_t *addrlen)
accept的socklen_t表示addr的实际长度
union sockaddr_all addr;
socklen_t addrlen = sizeof(addr);
socket_accept(sockfd, &addr, &addrlen);

fn(union sockaddr_all *addr, socklen_t addrlen)
当socklen_t代表当前sockaddr_all有效数据长度时候(区分当前sockaddr实际类型),
socklen_t要传递真实长度作为参数 sizeof(.s or .v4  or .v6),
而不能写sizeof(union sockaddr_all), 否则就变成最长的v6的地址长度了
可能会返回 Invalid Arguments
*/

```