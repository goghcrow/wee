#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include "sniff.h"
#include "../base/buffer.h"
#include "../base/endian.h"
#include "../base/queue.h"
#include "../base/log.h"

#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))
#endif

#define MYSQL_MAX_PACKET_LEN 0xFFFFFF

// 每个端口挂一个 struct conn(单向连接) 链表
#define PORT_QUEUE_SIZE 65535
static QUEUE PORT_QUEUE[PORT_QUEUE_SIZE];

static int16_t mysql_server_port;

struct conn
{
    uint32_t ip;   // 网络字节序
    uint16_t port; // 本机字节序
    struct buffer *buf;
    QUEUE node;
};

static struct conn *conn_create(uint32_t ip, uint16_t port)
{
    struct conn *c = malloc(sizeof(*c));
    if (c == NULL)
        return NULL;
    memset(c, 0, sizeof(*c));
    c->ip = ip;
    c->port = port;
    c->buf = buf_create(8192);
    return c;
}

static void conn_release(struct conn *c)
{
    assert(c);
    buf_release(c->buf);
    free(c);
}

static void pq_init()
{
    int i = 0;
    for (; i < PORT_QUEUE_SIZE; i++)
    {
        QUEUE_INIT(&PORT_QUEUE[i]);
    }
}

static void pq_release()
{
    QUEUE *q;
    QUEUE *el;
    struct conn *c;
    int i = 0;

    for (; i < PORT_QUEUE_SIZE; i++)
    {
        q = &PORT_QUEUE[i];
        if (QUEUE_EMPTY(q))
        {
            continue;
        }

        QUEUE_FOREACH(el, q)
        {
            c = QUEUE_DATA(el, struct conn, node);
            conn_release(c);
        }
    }
}

static void pq_dump()
{
    QUEUE *q;
    QUEUE *el;
    struct conn *c;
    int i = 0;
    char ip_buf[INET_ADDRSTRLEN];

    int bytes = 0;
    for (; i < PORT_QUEUE_SIZE; i++)
    {
        q = &PORT_QUEUE[i];
        if (QUEUE_EMPTY(q))
        {
            continue;
        }

        QUEUE_FOREACH(el, q)
        {
            c = QUEUE_DATA(el, struct conn, node);
            inet_ntop(AF_INET, &c->ip, ip_buf, INET_ADDRSTRLEN);
            printf("%s:%d %zu\n", ip_buf, c->port, buf_readable(c->buf));
            bytes += buf_internalCapacity(c->buf);
        }
        printf("bytes: %d\n", bytes);
    }
}

static struct conn *pq_get_internal(uint32_t ip, uint16_t port, int is_remove)
{
    QUEUE *q = &PORT_QUEUE[port];
    if (QUEUE_EMPTY(q))
    {
        return NULL;
    }

    QUEUE *el;
    struct conn *c;
    QUEUE_FOREACH(el, q)
    {
        c = QUEUE_DATA(el, struct conn, node);
        if (c->ip == ip)
        {
            if (is_remove)
            {
                QUEUE_REMOVE(el);
            }
            return c;
        }
    }
    return NULL;
}

static struct conn *pq_get(uint32_t ip, uint16_t port)
{
    return pq_get_internal(ip, port, 0);
}

struct conn *pq_del(uint32_t ip, uint16_t port)
{
    return pq_get_internal(ip, port, 1);
}

void pq_add(struct conn *c)
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

/* dissector helper: length of PDU */
static uint32_t
get_mysql_pdu_len(struct buffer *buf)
{
    int total_sz = buf_readable(buf);
    uint32_t pkt_sz = buf_readInt32LE24(buf);

    if ((total_sz - pkt_sz) == 7)
    {
        return pkt_sz + 7; /* compressed header 3+1+3 (len+id+cmp_len) */
    }
    else
    {
        return pkt_sz + 4; /* regular header 3+1 (len+id) */
    }
}

/*
 * Decode the header of a compressed packet
 * https://dev.mysql.com/doc/internals/en/compressed-packet-header.html
 */
static void
mysql_dissect_compressed_header(struct buffer *buf)
{
    int32_t cmp_pkt_sz = buf_readInt32LE24(buf);
    uint8_t cmp_pkt_num = buf_readInt8(buf);
    int32_t cmp_pkt_uncmp_sz = buf_readInt32LE24(buf);

    UNUSED(cmp_pkt_sz);
    UNUSED(cmp_pkt_num);
    UNUSED(cmp_pkt_uncmp_sz);
}

// TOOD 未处理 compressed header
bool is_completed_mysql_pdu(struct buffer *buf)
{
    if (buf_readable(buf) < 4) /* regular header 3+1 (len+id) */
    {
        return false;
    }
    int32_t sz = buf_peekInt32LE24(buf);
    if (sz <= 0 || sz >= MYSQL_MAX_PACKET_LEN)
    {
        PANIC("malformed mysql packet(size=%d)\n", sz);
        return false;
    }
    return buf_readable(buf) >= sz + 4;
}

void pkt_handle(void *ud,
                const struct pcap_pkthdr *pkt_hdr,
                const struct ip *ip_hdr,
                const struct tcphdr *tcp_hdr,
                const struct tcpopt *tcp_opt,
                const u_char *payload,
                size_t payload_size)
{
    // if (payload_size > 0)
    // {
    //     // print_bytes((char *)payload, payload_size);
    // }

    // char ip_buf[INET_ADDRSTRLEN];

    // printf("+-------------------------+\n");
    // printf("   ACK: %u\n", ntohl(tcp_hdr->th_ack));
    // printf("   SEQ: %u\n", ntohl(tcp_hdr->th_seq));

    // inet_ntop(AF_INET, &(ip_hdr->ip_dst.s_addr), ip_buf, INET_ADDRSTRLEN);
    // printf("   DST IP: %s\n", ip_buf);
    // inet_ntop(AF_INET, &(ip_hdr->ip_src.s_addr), ip_buf, INET_ADDRSTRLEN);
    // printf("   SRC IP: %s\n", ip_buf);

    // printf("   SRC PORT: %d\n", ntohs(tcp_hdr->th_sport));
    // printf("   DST PORT: %d\n", ntohs(tcp_hdr->th_dport));

    struct conn *c;
    uint32_t s_ip = ip_hdr->ip_src.s_addr;
    uint16_t s_port = ntohs(tcp_hdr->th_sport);

    // 连接关闭, 清理数据
    if (tcp_hdr->th_flags & TH_FIN || tcp_hdr->th_flags & TH_RST)
    {
        char s_ip_buf[INET_ADDRSTRLEN];
        char d_ip_buf[INET_ADDRSTRLEN];
        // uint32_t d_ip = ip_hdr->ip_dst.s_addr;
        uint16_t d_port = ntohs(tcp_hdr->th_dport);

        inet_ntop(AF_INET, &(ip_hdr->ip_src.s_addr), s_ip_buf, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(ip_hdr->ip_dst.s_addr), d_ip_buf, INET_ADDRSTRLEN);

        LOG_INFO("%s:%d %s:%d 关闭连接\n", s_ip_buf, s_port, d_ip_buf, d_port);
        c = pq_del(s_ip, s_port);
        if (c)
        {
            conn_release(c);
        }
        c = pq_del(ip_hdr->ip_dst.s_addr, ntohs(tcp_hdr->th_dport));
        if (c)
        {
            conn_release(c);
        }
    }

    if (payload_size <= 0)
    {
        return;
    }

    // 这里假定一定是 mysql 数据 !!!
    // 获取或初始化连接对象
    c = pq_get(s_ip, s_port);
    if (c == NULL)
    {
        c = conn_create(s_ip, s_port);
        pq_add(c);
    }

    struct buffer *buf = c->buf;
    buf_append(buf, (const char *)payload, payload_size);
    
    if (!is_completed_mysql_pdu(buf)) {
        return;
    }
    
    bool is_response = false;
    if (s_port == mysql_server_port) {
        is_response = true;
    }
    
    int32_t pkt_sz = buf_readInt32LE24(buf);
    uint8_t pkt_id = buf_readInt8(buf); 

    LOG_INFO("packet size %d\n", pkt_sz);
    LOG_INFO("packet id %d\n", pkt_id);
   
    // TODO 检测是否是 SSL !!!

    if (is_response) {
        LOG_INFO("RESPONSE\n");
    } else {
        LOG_INFO("REQUEST\n");
    }

    if (buf_internalCapacity(buf) > 1024 * 1024)
    {
        buf_shrink(c->buf, 0);
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

    mysql_server_port = 3306;

    struct tcpsniff_opt opt = {
        .snaplen = 65535,
        .pkt_cnt_limit = 0,
        .timeout_limit = 10,
        // .device = device,
        .device = "lo0",
        .filter_exp = "tcp and port 3306",
        .ud = NULL};


    pq_init();

    if (!tcpsniff(&opt, pkt_handle))
    {
        fprintf(stderr, "fail to sniff\n");
    }
    return 0;
}