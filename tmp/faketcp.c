#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if_tun.h>

// http://man7.org/linux/man-pages/man7/netdevice.7.html

int sethostaddr(const char *dev, const char *ip)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, dev);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    memmove(&ifr.ifr_addr, &addr, sizeof(addr));

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR socket");
        return -1;
    }

    // ifconfig tun0 192.168.0.1
    int err = ioctl(sockfd, SIOCSIFADDR, (void *)&ifr);
    if (err < 0)
    {
        perror("ERROR ioctl SIOCSIFADDR");
        close(sockfd);
        return err;
    }

    // ifup tun0
    err = ioctl(sockfd, SIOCGIFFLAGS, (void *)&ifr);
    if (err < 0)
    {
        perror("ERROR ioctl SIOCGIFFLAGS");
        close(sockfd);
        return err;
    }

    ifr.ifr_flags |= IFF_UP;
    err = ioctl(sockfd, SIOCSIFFLAGS, (void *)&ifr);
    if (err < 0)
    {
        perror("ERROR ioctl SIOCSIFFLAGS");
        close(sockfd);
        return err;
    }

    // ifconfig tun0 192.168.0.1/24
    inet_pton(AF_INET, "255.255.255.0", &addr.sin_addr);
    memmove(&ifr.ifr_netmask, &addr, sizeof(addr));
    err = ioctl(sockfd, SIOCSIFNETMASK, (void *)&ifr);
    if (err < 0)
    {
        perror("ERROR ioctl SIOCSIFNETMASK");
        close(sockfd);
        return err;
    }

    close(sockfd);
    return err;
}

// sz = IFNAMSIZ
int tun_alloc(char *dev, size_t sz, const char *ip)
{
    struct ifreq ifr;

    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0)
    {
        perror("ERROR open /dev/net/tun");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (*dev)
    {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    int err = ioctl(fd, TUNSETIFF, (void *)&ifr);
    if (err < 0)
    {
        perror("ERROR ioctl TUNSETIFF");
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);
    err = sethostaddr(dev, ip);
    if (err < 0)
    {
        return err;
    }

    return fd;
}

uint16_t in_checksum(char *buf, int len)
{
    assert(len % 2 == 0);
    const uint16_t *data = (uint16_t *)buf;
    int sum = 0;
    int i;
    for (i = 0; i < len; i += 2)
    {
        sum += *data++;
    }
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    assert(sum <= 0xFFFF);
    return ~sum;
}

void icmp_input(int fd, const char *input, const void *payload, int len)
{
    struct iphdr *iphdr = (struct iphdr *)input;
    struct icmphdr *icmphdr = (struct icmphdr *)payload;
    const int iphdr_len = iphdr->ihl * 4;

    if (icmphdr->type == ICMP_ECHO)
    {
        char source[INET_ADDRSTRLEN];
        char dest[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &iphdr->saddr, source, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &iphdr->daddr, dest, INET_ADDRSTRLEN);

        printf("%s > %s: ", source, dest);
        printf("ICMP echo request, id %d, seq %d, length %d\n",
               ntohs(icmphdr->un.echo.id),
               ntohs(icmphdr->un.echo.sequence),
               len - iphdr_len);

        union {
            unsigned char output[ETH_FRAME_LEN];
            struct
            {
                struct iphdr iphdr;
                struct icmphdr icmphdr;
            } out;
        } u;

        memcpy(u.output, input, len);
        u.out.icmphdr.type = ICMP_ECHOREPLY;
        u.out.icmphdr.checksum += ICMP_ECHO; // FIXME: not portable
        uint32_t addr = u.out.iphdr.saddr;
        u.out.iphdr.saddr = u.out.iphdr.daddr;
        u.out.iphdr.daddr = addr;
        write(fd, u.output, len);
    }
}


/*
1. 在第 1 个窗口运行 sudo ./icmpecho ，程序显示
allocted tunnel interface tun0

2. 在第 2 个窗口运行
$ sudo ifconfig tun0 192.168.0.1/24
$ sudo tcpdump -i tun0

3. 在第 3 个窗口运行
$ ping 192.168.0.2
$ ping 192.168.0.3
$ ping 192.168.0.234
发现每个 192.168.0.X 的 IP 都能 ping 通
*/
int icmpecho()
{
    char ifname[IFNAMSIZ] = "tun%d";
    const char *ip = "192.168.0.1";
    int fd = tun_alloc(ifname, IFNAMSIZ, ip);

    if (fd < 0)
    {
        fprintf(stderr, "tunnel interface allocation failed\n");
        _exit(1);
    }

    printf("allocted tunnel interface %s\n", ifname);
    sleep(1);

    for (;;)
    {
        union {
            unsigned char buf[ETH_FRAME_LEN];
            struct iphdr iphdr;
        } u;

        const int iphdr_size = sizeof(u.iphdr);
        int nread = read(fd, u.buf, sizeof(u.buf));
        if (nread < 0)
        {
            perror("ERROR read");
            close(fd);
            _exit(1);
        }
        printf("read %d bytes from tunnel interface %s.\n", nread, ifname);

        const int iphdr_len = u.iphdr.ihl * 4;

        if (nread >= iphdr_size &&
            u.iphdr.version == 4 &&
            iphdr_len >= iphdr_size &&
            iphdr_len <= nread &&
            u.iphdr.tot_len == htons(nread) &&
            in_checksum(u.buf, iphdr_len) == 0)
        {
            const void *payload = u.buf + iphdr_len;
            if (u.iphdr.protocol == IPPROTO_ICMP)
            {
                icmp_input(fd, u.buf, payload, nread);
            }
        }
        else
        {
            printf("bad packet\n");
            int i;
            for (i = 0; i < nread; ++i)
            {
                if (i % 4 == 0)
                    printf("\n");
                printf("%02x ", u.buf[i]);
            }
            printf("\n");
        }
    }

    return 0;
}

int main(void)
{
    
}