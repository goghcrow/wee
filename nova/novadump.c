#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../net/sniff.h"
#include "../base/buffer.h"
#include "../base/queue.h"
#include "novacodec.h"

struct conn
{
    uint32_t ip;
    uint16_t port;
    struct buffer *buf;
    QUEUE node;
};

static struct conn *conn_create(uint32_t ip, uint16_t port)
{
    struct conn *c = malloc(sizeof(*c));
    assert(c);
    memset(c, 0, sizeof(*c));
    c->ip = ip;
    c->port = port;
    c->buf = buf_create(1024);
    return c;
}

static void conn_release(struct conn *c)
{
    assert(c);
    buf_release(c->buf);
    free(c);
}

static QUEUE PORT_QUEUE[65536];

static void pq_init()
{
    int i = 0;
    for (; i < 65536; i++)
    {
        QUEUE_INIT(&PORT_QUEUE[i]);
    }
}

static struct conn *pq_get_remove(uint32_t ip, uint16_t port, int remove)
{
    QUEUE *q = &PORT_QUEUE[port];
    if (QUEUE_EMPTY(q))
    {
        return NULL;
    }
    else
    {
        QUEUE *el;
        struct conn *c;
        QUEUE_FOREACH(el, q)
        {
            c = QUEUE_DATA(el, struct conn, node);
            if (c->ip == ip)
            {
                if (remove)
                {
                    QUEUE_REMOVE(el);
                }
                return c;
            }
        }
        return NULL;
    }
}

static struct conn *pq_get(uint32_t ip, uint16_t port)
{
    return pq_get_remove(ip, port, 0);
}

struct conn *pq_del(uint32_t ip, uint16_t port)
{
    return pq_get_remove(ip, port, 1);
}

void pq_set(struct conn *c)
{
    QUEUE *q = &PORT_QUEUE[c->port];
    if (pq_get(c->ip, c->port) == NULL)
    {
        QUEUE_INSERT_TAIL(q, &c->node);
    }
    else
    {
        assert(false);
    }
}

void pkt_handle(void *ud,
                const struct pcap_pkthdr *pkt_hdr,
                const struct ip *ip_hdr,
                const struct tcphdr *tcp_hdr,
                const u_char *payload, size_t payload_size)
{

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

    struct conn *c;
    uint32_t ip = ip_hdr->ip_src.s_addr;
    uint16_t port = tcp_hdr->th_sport;

    if (tcp_hdr->th_flags & TH_FIN || tcp_hdr->th_flags & TH_RST)
    {
        c = pq_del(ip, port);
        if (c)
        {
            conn_release(c);
        }
        c = pq_del(ip_hdr->ip_dst.s_addr, tcp_hdr->th_dport);
        if (c)
        {
            conn_release(c);
        }
    }
    else if (/*(tcp_hdr->th_flags & TH_PUSH) &&*/ payload_size)
    {

        c = pq_get(ip, port);
        if (c == NULL)
        {
            if (nova_detect((const char *)payload, payload_size))
            {
                c = conn_create(ip, port);
                pq_set(c);
            }
            else
            {
                // 非nova数据
                return;
            }
        }
        buf_append(c->buf, (const char *)payload, payload_size);
        if (buf_peekInt32(c->buf) > buf_readable(c->buf))
        {
            // 数据不足一个nova包
            return;
        }

        struct nova_hdr *nova_hdr = nova_hdr_create();
        if (!nova_unpack(c->buf, nova_hdr))
        {
            fprintf(stderr, "nova_unpack fail\n");
            c = pq_del(ip, port);
            conn_release(c);
            nova_hdr_release(nova_hdr);
            return;
        }

        char t1[nova_hdr->service_len + 1];
        memcpy(t1, nova_hdr->service_name, nova_hdr->service_len);
        t1[nova_hdr->service_len] = 0;
        printf("service=%s\n", t1);

        char t2[nova_hdr->method_len + 1];
        memcpy(t2, nova_hdr->method_name, nova_hdr->method_len);
        t2[nova_hdr->method_len] = 0;
        printf("method=%s\n", t2);

        // TODO
        nova_hdr_release(nova_hdr);
    }
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

    pq_init();

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
        exit(1);
    }
    return 0;
}