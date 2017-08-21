#include <stdio.h>
#include <arpa/inet.h>
#include "tcpsniff.h"

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

void pkt_handle(void *ud,
                const struct pcap_pkthdr *pkt_hdr,
                const struct ip *ip_hdr,
                const struct tcphdr *tcp_hdr,
                const u_char *payload, size_t payload_size)
{
    if (payload_size > 0)
    {
        print_bytes((char *)payload, payload_size);
    }

    // AND RST
    if (tcp_hdr->th_flags & TH_FIN) {
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    }

    char ip_buf[INET_ADDRSTRLEN];

    printf("+-------------------------+\n");
    printf("   ACK: %u\n", ntohl(tcp_hdr->th_ack));
    printf("   SEQ: %u\n", ntohl(tcp_hdr->th_seq));

    inet_ntop(AF_INET, &(ip_hdr->ip_dst.s_addr), ip_buf, INET_ADDRSTRLEN);
    printf("   DST IP: %s\n", ip_buf);
    inet_ntop(AF_INET, &(ip_hdr->ip_src.s_addr), ip_buf, INET_ADDRSTRLEN);
    printf("   SRC IP: %s\n", ip_buf);

    printf("   SRC PORT: %d\n", ntohs(tcp_hdr->th_sport));
    printf("   DST PORT: %d\n", ntohs(tcp_hdr->th_dport));
}

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

    struct tcpsniff_opt opt = {
        .snaplen = 65535,
        .pkt_cnt_limit = 0,
        .timeout_limit = 10,
        .device = device,
        .filter_exp = "tcp",
        .ud = NULL};

    if (!tcpsniff(&opt, pkt_handle))
    {
        fprintf(stderr, "fail to sniff\n");
    }
    return 0;
}