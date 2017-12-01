#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include "mysql_sniff.h"
#include "../net/sniff.h"
#include "../base/buffer.h"
#include "../base/endian.h"
#include "../base/queue.h"

/*
TODO:
1. sannitizer 测试
2. charset format
3. 处理掉 buf_readCStr buf_readStr
3. 比较ip大小, 做一个会话struct, 挂一个 mysql_conn_data, 两个方向的 buffer

*/

#if !defined(UNUSED)
#define UNUSED(x) ((void)(x))
#endif

// 每个端口挂一个 struct conn 链表,
// 简单处理, conn 代表半个 tcp 回话, 单方向连接
// 每个 struct conn 挂一个 buffer 缓存接受数据, 用来处理粘包,半包
#define PORT_QUEUE_SIZE 65535
static QUEUE PORT_QUEUE[PORT_QUEUE_SIZE];

static int16_t mysql_server_port;
static struct mysql_conn_data g_conn_data;
// #define BUFDZ MYSQL_MAX_PACKET_LEN
#define BUFSZ 1024 * 1024
static char g_buf[BUFSZ];


// TODO
struct mysql_session {
    // src, dst ip port
    // src buf, dst buf
    // mysql_conn_data

};

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

// malloc free 全部修改为 栈内存.... 反正不需要保存, 直接打印到控制台好了
static int buf_strsize(struct buffer *buf);
static uint64_t buf_readFle(struct buffer *buf, uint64_t *len, uint8_t *is_null);
static int buf_peekFleLen(struct buffer *buf);
static int buf_dupFleStr(struct buffer *buf, char **str);

static uint32_t get_mysql_pdu_len(struct buffer *buf);
static void mysql_dissect_compressed_header(struct buffer *buf);
bool is_completed_mysql_pdu(struct buffer *buf);

static void mysql_dissect_auth_switch_response(struct buffer *buf, mysql_conn_data_t *conn_data);
static void mysql_dissect_error_packet(struct buffer *buf);
static void mysql_set_conn_state(mysql_conn_data_t *conn_data, mysql_state_t state);
static void mysql_dissect_greeting(struct buffer *buf, mysql_conn_data_t *conn_data);
static void mysql_dissect_login(struct buffer *buf, mysql_conn_data_t *conn_data);
static int mysql_dissect_attributes(struct buffer *buf);
static void mysql_dissect_request(struct buffer *buf, mysql_conn_data_t *conn_data);
static void mysql_dissect_response(struct buffer *buf, mysql_conn_data_t *conn_data);
static void mysql_dissect_result_header(struct buffer *buf, mysql_conn_data_t *conn_data);
static void mysql_dissect_ok_packet(struct buffer *buf, mysql_conn_data_t *conn_data);
static void mysql_dissect_field_packet(struct buffer *buf, mysql_conn_data_t *conn_data);
static int mysql_dissect_session_tracker_entry(struct buffer *buf);
static void mysql_dissect_row_packet(struct buffer *buf);

void pkt_handle(void *ud,
                const struct pcap_pkthdr *pkt_hdr,
                const struct ip *ip_hdr,
                const struct tcphdr *tcp_hdr,
                const struct tcpopt *tcp_opt,
                const u_char *payload,
                size_t payload_size)
{
    static char s_ip_buf[INET_ADDRSTRLEN];
    static char d_ip_buf[INET_ADDRSTRLEN];

    struct conn *c;

    uint32_t s_ip = ip_hdr->ip_src.s_addr;
    uint16_t s_port = ntohs(tcp_hdr->th_sport);

    uint32_t d_ip = ip_hdr->ip_dst.s_addr;
    uint16_t d_port = ntohs(tcp_hdr->th_dport);

    inet_ntop(AF_INET, &(ip_hdr->ip_src.s_addr), s_ip_buf, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip_hdr->ip_dst.s_addr), d_ip_buf, INET_ADDRSTRLEN);

    printf("%s:%d > %s:%d ack %u, seq %u, sz %zd\n", s_ip_buf, s_port, d_ip_buf, d_port,
           ntohl(tcp_hdr->th_ack), ntohl(tcp_hdr->th_seq), payload_size);

    // 连接关闭, 清理数据
    if (tcp_hdr->th_flags & TH_FIN || tcp_hdr->th_flags & TH_RST)
    {
        LOG_INFO("%s:%d > %s:%d Close Connection", s_ip_buf, s_port, d_ip_buf, d_port);
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
        return;
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

    bool is_response = false;
    if (s_port == mysql_server_port)
    {
        is_response = true;
    }

    // 一个 tcp segment 可能包括 N 个 Mysql Packet, 比如 select查询结果集
    while (is_completed_mysql_pdu(buf))
    {
        int32_t pkt_sz = buf_readInt32LE24(buf);
        uint8_t pkt_num = buf_readInt8(buf);
        LOG_INFO("%s:%d > %s:%d pkt_sz %d, pkt_no %d", s_ip_buf, s_port, d_ip_buf, s_port, pkt_sz, pkt_num);

        // 这里不用担心频繁创建只读视图, 内部有缓存
        struct buffer *rbuf = buf_readonlyView(buf, pkt_sz);
        buf_retrieve(buf, pkt_sz);

        // TODO 检测是否是 SSL !!!
        bool is_ssl = false;

        if (is_response)
        {
            if (pkt_num == 0 && g_conn_data.state == UNDEFINED)
            {
                mysql_dissect_greeting(rbuf, &g_conn_data);
            }
            else
            {
                mysql_dissect_response(rbuf, &g_conn_data);
            }
        }
        else
        {
            // TODO 这里 有问题, 暂时没进入该分支 !!!!, 抓取不到 login
            if (g_conn_data.state == LOGIN && (pkt_num == 1 || (pkt_num == 2 && is_ssl)))
            {
                mysql_dissect_login(rbuf, &g_conn_data);
                if (pkt_num == 2 && is_ssl)
                {
                    PANIC("SLL NOT SUPPORT");
                }
                // TODO 处理 SSL
                /*
                if ((g_conn_data.srv_caps & MYSQL_CAPS_CP) && (g_conn_data.clnt_caps & MYSQL_CAPS_CP))
                {
                    g_conn_data.frame_start_compressed = pinfo->num;
                    g_conn_data.compressed_state = MYSQL_COMPRESS_INIT;
                }
                */
            }
            else
            {
                mysql_dissect_request(rbuf, &g_conn_data);
            }
        }
        buf_release(rbuf);
    }

    if (buf_internalCapacity(buf) > 1024 * 1024)
    {
        buf_shrink(c->buf, 0);
    }
}

void init()
{
    pq_init();

    g_conn_data.srv_caps = 0;
    g_conn_data.clnt_caps = 0;
    g_conn_data.clnt_caps_ext = 0;
    g_conn_data.state = UNDEFINED;
    // g_conn_data.stmts =
    g_conn_data.major_version = 0;
    g_conn_data.frame_start_ssl = 0;
    g_conn_data.frame_start_compressed = 0;
    g_conn_data.compressed_state = MYSQL_COMPRESS_NONE;
}

void release()
{
    pq_release();
}

int main(int argc, char **argv)
{
    char *device;
    if (argc < 2)
    {
        device = "lo0";
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

    init();

    if (!tcpsniff(&opt, pkt_handle))
    {
        fprintf(stderr, "fail to sniff\n");
        release();
    }
    return 0;
}

static const char *
val_to_str(const struct val_str *val_strs, uint32_t val, char *def)
{
    struct val_str *p = (struct val_str *)val_strs - 1;
    while ((++p)->str)
    {
        if (p->val == val)
        {
            return p->str;
        }
    }
    return def;
}

static const char *
mysql_get_command(uint32_t val, char *def)
{
    return val_to_str(mysql_command_vals, val, def);
}

static const char *
mysql_get_charset(uint32_t val, char *def)
{
    return val_to_str(mysql_collation_vals, val, def);
}

static const char*
mysql_get_field_type(uint32_t val, char *def) {
    return val_to_str(type_constants, val, def);
}

// mysql_collation_vals

static int buf_strsize(struct buffer *buf)
{
    const char *eos = buf_findChar(buf, '\0');
    if (eos == NULL)
    {
        return buf_readable(buf);
    }
    else
    {
        return eos - buf_peek(buf);
    }
}

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
        PANIC("Malformed Mysql Packet (size=%d)\n", sz);
        return false;
    }
    return buf_readable(buf) >= sz + 4;
}

/**
Value Of     # Of Bytes  Description
First Byte   Following
----------   ----------- -----------
0-250        0           = value of first byte
251          0           column value = NULL
only appropriate in a Row Data Packet
252          2           = value of following 16-bit word
253          3           = value of following 24-bit word
254          8           = value of following 64-bit word
*/
// One may ask why the 1 byte length is limited to 251, when the first reserved value in the
// net_store_length( ) is 252. The code 251 has a special meaning. It indicates that there is
// no length value or data following the code, and the value of the field is the SQL

// field length encoded
// len     out
// is_null out   where to store ISNULL flag, may be NULL
// return where to store FLE value, may be NULL
static uint64_t
buf_readFle(struct buffer *buf, uint64_t *len, uint8_t *is_null)
{
    uint8_t prefix = buf_readInt8(buf);

    if (is_null)
    {
        *is_null = 0;
    }

    switch (prefix)
    {
    case 251:
        if (len)
        {
            *len = 1;
        }
        if (is_null)
        {
            *is_null = 1;
        }
        return 0;
    case 252:
        if (len)
        {
            *len = 1 + 2;
        }
        return buf_readInt16LE(buf);
    case 253:
        if (len)
        {
            *len = 1 + 4;
        }
        return buf_readInt32LE(buf);

        // TODO 好像有种情况是这样 !!!
        /*
		if (len) {
			*len = 1 + 3;
		}
		return buf_readInt32LE24(buf);
		*/
    case 254:
        if (len)
        {
            *len = 1 + 8;
        }
        return buf_readInt64LE(buf);
    default: /* < 251 */
        if (len)
        {
            *len = 1;
        }
        return prefix;
    }
}

static int
buf_peekFleLen(struct buffer *buf)
{
    uint8_t prefix = buf_readInt8(buf);

    switch (prefix)
    {
    case 251:
        return 1;
    case 252:
        return 1 + 2;
    case 253:
        return 1 + 4;
        // TODO
        return 1 + 3;
    case 254:
        return 1 + 8;
    default: /* < 251 */
        return 1;
    }
}

static int
buf_dupFleStr(struct buffer *buf, char **str)
{
    uint64_t len;
    uint64_t sz = buf_readFle(buf, &len, NULL);
    *str = buf_dupStr(buf, sz);
    return len + sz;
}

static int
buf_readFleStr(struct buffer *buf, char *str, int sz)
{
    uint64_t len;
    uint64_t sz1 = buf_readFle(buf, &len, NULL);
    if (sz1 > sz)
    {
        assert(false);
        return -1;
    }

    buf_readStr(buf, str, sz1);
    return len + sz1;
}

/*
参考文档
https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_PROTOCOL.html
https://dev.mysql.com/doc/internals/en/client-server-protocol.html
http://hutaow.com/blog/2013/11/06/mysql-protocol-analysis/
wireshare 源码

Server Status: 0x0002
.... .... .... ...0 = In transaction: Not set
.... .... .... ..1. = AUTO_COMMIT: Set
.... .... .... .0.. = More results: Not set
.... .... .... 0... = Multi query - more resultsets: Not set
.... .... ...0 .... = Bad index used: Not set
.... .... ..0. .... = No index used: Not set
.... .... .0.. .... = Cursor exists: Not set
.... .... 0... .... = Last row sent: Not set
.... ...0 .... .... = database dropped: Not set
.... ..0. .... .... = No backslash escapes: Not set
.... .0.. .... .... = Session state changed: Not set
.... 0... .... .... = Query was slow: Not set
...0 .... .... .... = PS Out Params: Not set


Server Capabilities: 0xffff
.... .... .... ...1 = Long Password: Set
.... .... .... ..1. = Found Rows: Set
.... .... .... .1.. = Long Column Flags: Set
.... .... .... 1... = Connect With Database: Set
.... .... ...1 .... = Don't Allow database.table.column: Set
.... .... ..1. .... = Can use compression protocol: Set
.... .... .1.. .... = ODBC Client: Set
.... .... 1... .... = Can Use LOAD DATA LOCAL: Set
.... ...1 .... .... = Ignore Spaces before '(': Set
.... ..1. .... .... = Speaks 4.1 protocol (new flag): Set
.... .1.. .... .... = Interactive Client: Set
.... 1... .... .... = Switch to SSL after handshake: Set
...1 .... .... .... = Ignore sigpipes: Set
..1. .... .... .... = Knows about transactions: Set
.1.. .... .... .... = Speaks 4.1 protocol (old flag): Set
1... .... .... .... = Can do 4.1 authentication: Set

Extended Server Capabilities: 0xc1ff
.... .... .... ...1 = Multiple statements: Set
.... .... .... ..1. = Multiple results: Set
.... .... .... .1.. = PS Multiple results: Set
.... .... .... 1... = Plugin Auth: Set
.... .... ...1 .... = Connect attrs: Set
.... .... ..1. .... = Plugin Auth LENENC Client Data: Set
.... .... .1.. .... = Client can handle expired passwords: Set
.... .... 1... .... = Session variable tracking: Set
.... ...1 .... .... = Deprecate EOF: Set
1100 000. .... .... = Unused: 0x60

Client Capabilities: 0x8208
.... .... .... ...0 = Long Password: Not set
.... .... .... ..0. = Found Rows: Not set
.... .... .... .0.. = Long Column Flags: Not set
.... .... .... 1... = Connect With Database: Set
.... .... ...0 .... = Don't Allow database.table.column: Not set
.... .... ..0. .... = Can use compression protocol: Not set
.... .... .0.. .... = ODBC Client: Not set
.... .... 0... .... = Can Use LOAD DATA LOCAL: Not set
.... ...0 .... .... = Ignore Spaces before '(': Not set
.... ..1. .... .... = Speaks 4.1 protocol (new flag): Set
.... .0.. .... .... = Interactive Client: Not set
.... 0... .... .... = Switch to SSL after handshake: Not set
...0 .... .... .... = Ignore sigpipes: Not set
..0. .... .... .... = Knows about transactions: Not set
.0.. .... .... .... = Speaks 4.1 protocol (old flag): Not set
1... .... .... .... = Can do 4.1 authentication: Set

Extended Client Capabilities: 0x0008
.... .... .... ...0 = Multiple statements: Not set
.... .... .... ..0. = Multiple results: Not set
.... .... .... .0.. = PS Multiple results: Not set
.... .... .... 1... = Plugin Auth: Set
.... .... ...0 .... = Connect attrs: Not set
.... .... ..0. .... = Plugin Auth LENENC Client Data: Not set
.... .... .0.. .... = Client can handle expired passwords: Not set
.... .... 0... .... = Session variable tracking: Not set
.... ...0 .... .... = Deprecate EOF: Not set
0000 000. .... .... = Unused: 0x00


Uint8 						0x0a		Protocol
NULL-terminated-str 					Banner
uint32LE								ThreadId
NULL-terminated-str						Salt 用于客户端加密密码
UInt16LE					0xffff		Server Capabilities
UInt8LE						33			Server Language, Charset, 33: utf8 COLLATE utf8_general_ci
UInt16LE					0x0002		Server Status, 0x0002 status autommit
UInt16LE					0x0008		Extended Server Capalibities
Uint8						21			Authentication Plugin Length, 21 = strlen(mysql_native_password)
10bytes						Unused		str_repeat("\0", 10)
NULL-terminated-str			具体盐值	 salt		
NULL-terminated-str			"mysql_native_password\0"	Authentication Plugin
*/
static void
mysql_dissect_greeting(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    LOG_INFO("Server Greeting");
    int protocol = buf_readInt8(buf);
    if (protocol == 0xff)
    {
        mysql_dissect_error_packet(buf);
        return;
    }

    mysql_set_conn_state(conn_data, LOGIN);
    LOG_INFO("Protocol 0x%02x", protocol);

    // null 结尾字符串, Banner
    buf_readCStr(buf, g_buf, BUFSZ);
    LOG_INFO("Server Version: %s", g_buf);

    // TODO 获取 major version
    /* version string */
    conn_data->major_version = 0;
    // char * eos = buf_findChar(buf, '\0');
    // assert(eos);
    // int lenstr = eos - buf_peek(buf);
    // int ver_offset;
    // for (ver_offset = 0; ver_offset < lenstr; ver_offset++) {
    // 	guint8 ver_char = tvb_get_guint8(tvb, offset + ver_offset);
    // 	if (ver_char == '.') break;
    // 	conn_data->major_version = conn_data->major_version * 10 + ver_char - '0';
    // }

    /* 4 bytes little endian thread_id */
    int thread_id = buf_readInt32LE(buf);
    LOG_INFO("Server Thread Id %d", thread_id);

    /* salt string */
    buf_readCStr(buf, g_buf, BUFSZ);
    // LOG_INFO("Salt %s", g_buf);

    /* rest is optional */
    if (!buf_readable(buf))
    {
        return;
    }

    /* 2 bytes CAPS */
    conn_data->srv_caps = buf_readInt16LE(buf);
    LOG_INFO("Server Capabilities 0x%04x", conn_data->srv_caps);
    /* rest is optional */
    if (!buf_readable(buf))
    {
        return;
    }

    // TODO 打印信息

    /* 1 byte Charset */
    int8_t charset = buf_readInt8(buf);
    LOG_INFO("Server Charset [%s](0x%02x)", mysql_get_charset(charset, "未知编码"), charset);
    

    /* 2 byte ServerStatus */
    int16_t server_status = buf_readInt16LE(buf);
    LOG_INFO("Server Statue 0x%04x", server_status);

    /* 2 bytes ExtCAPS */
    conn_data->srv_caps_ext = buf_readInt16LE(buf);
    LOG_INFO("Server Extended Capabilities 0x%04x", conn_data->srv_caps_ext);

    /* 1 byte Auth Plugin Length */
    int8_t auto_plugin_len = buf_readInt8(buf);

    /* 10 bytes unused */
    buf_retrieve(buf, 10);

    /* 4.1+ server: rest of salt */
    if (buf_readable(buf))
    {
        buf_readCStr(buf, g_buf, BUFSZ);
        // LOG_INFO("Server Rest Salt %s", g_buf);
    }

    /* 5.x server: auth plugin */
    if (buf_readable(buf))
    {
        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Server Auth Plugin: %s", g_buf);
    }
}

static void
mysql_dissect_error_packet(struct buffer *buf)
{
    int16_t errno = buf_readInt16LE(buf);
    LOG_ERROR("Error Code %d", errno);

    char *sqlstate;
    const char *c = buf_peek(buf);
    if (*c == '#')
    {
        buf_retrieve(buf, 1);
        buf_readStr(buf, g_buf, 5);
        LOG_ERROR("SQL State %s", g_buf);
    }
    buf_readStr(buf, g_buf, BUFSZ);
    LOG_ERROR("Error Message: %s", g_buf);
}

static void
mysql_set_conn_state(mysql_conn_data_t *conn_data, mysql_state_t state)
{
    conn_data->state = state;
}

static void
mysql_dissect_login(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    LOG_INFO("Login Request");

    int lenstr;

    /* after login there can be OK or DENIED */
    mysql_set_conn_state(conn_data, RESPONSE_OK);

    /*
UInt16LE				Client Capabilities
UInt16LE				Extended Client Capabilities
UInt32LE				MAX Packet: e.g. 300
UInt8					Charset: utf8 COLLATE utf8_general_ci (33)
Unused		 			23 Bytes 0x00
NullTerminatedString	Username: e.g. root
UInt8LenString			Password: e.g. 71f31c52cab00272caa32423f1714464113b7819
NullTerminatedString	Schema: e.g. test DB
NullTerminatedString	Client Auth Plugin: e.g. mysql_native_password
					* connection attributes
*/

    conn_data->clnt_caps = buf_readInt16LE(buf);
    LOG_INFO("Client Capabilities 0x%04x", conn_data->clnt_caps);

    /* Next packet will be use SSL */
    if (!(conn_data->frame_start_ssl) && conn_data->clnt_caps & MYSQL_CAPS_SL)
    {
        // "Response: SSL Handshake"
        // conn_data->frame_start_ssl = pinfo->num;
        // ssl_starttls_ack(ssl_handle, pinfo, mysql_handle);
    }

    uint32_t maxpkt;
    uint8_t charset;
    /* 4.1 protocol */
    if (conn_data->clnt_caps & MYSQL_CAPS_CU)
    {
        /* 2 bytes client caps */
        conn_data->clnt_caps_ext = buf_readInt16LE(buf);
        LOG_INFO("Client Extended Capabilities 0x%04x", conn_data->clnt_caps_ext);

        /* 4 bytes max package */
        maxpkt = buf_readInt32LE(buf);
        LOG_INFO("Client Max Packet %d", maxpkt);

        /* 1 byte Charset */
        charset = buf_readInt8(buf);
        LOG_INFO("Client Charset [%s](0x%02x)", mysql_get_charset(charset, "未知编码"), charset);

        /* filler 23 bytes */
        buf_retrieve(buf, 23);
    }
    else
    { /* pre-4.1 */
        /* 3 bytes max package */
        maxpkt = buf_readInt32LE24(buf);
        LOG_INFO("Client Max Packet %d", maxpkt);
    }

    /* User name */
    buf_readCStr(buf, g_buf, BUFSZ);
    LOG_INFO("Client User %s", g_buf);

    /* rest is optional */
    if (!buf_readable(buf))
    {
        return;
    }

    /* 两种情况: password: ascii or length+ascii */
    if (conn_data->clnt_caps & MYSQL_CAPS_SC)
    {
        uint8_t lenstr = buf_readInt8(buf);
        buf_readStr(buf, g_buf, lenstr);
    }
    else
    {
        buf_readCStr(buf, g_buf, BUFSZ);
    }
    // LOG_INFO("Client Password %s", g_buf);

    if (!buf_readable(buf))
    {
        return;
    }

    /* optional: initial schema */
    if (conn_data->clnt_caps & MYSQL_CAPS_CD)
    {
        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Client Schema %s", g_buf);
    }

    /* optional: authentication plugin */
    if (conn_data->clnt_caps_ext & MYSQL_CAPS_PA)
    {
        mysql_set_conn_state(conn_data, AUTH_SWITCH_REQUEST);

        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Client Auth Plugin %s", g_buf);
    }

    /* optional: connection attributes */
    if (conn_data->clnt_caps_ext & MYSQL_CAPS_CA && buf_readable(buf))
    {
        uint64_t connattrs_length = buf_readFle(buf, NULL, NULL);
        while (connattrs_length > 0)
        {
            int length = mysql_dissect_attributes(buf);
            connattrs_length -= length;
        }
    }
}

static int
mysql_dissect_attributes(struct buffer *buf)
{
    int lenfle;

    char *mysql_connattrs_name = NULL;
    char *mysql_connattrs_value = NULL;

    int name_len = buf_dupFleStr(buf, &mysql_connattrs_name);
    int val_len = buf_dupFleStr(buf, &mysql_connattrs_value);
    LOG_INFO("Client Attributes %s %s", mysql_connattrs_name, mysql_connattrs_value);
    free(mysql_connattrs_name);
    free(mysql_connattrs_value);
    return name_len + val_len;
}

static void
mysql_dissect_auth_switch_response(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    int lenstr;
    LOG_INFO("Auth Switch Response");

    /* Data */
    char *data = buf_dupCStr(buf);
    LOG_INFO("%s", data);
    free(data);
}

/*, packet_info *pinfo, */
static void
mysql_dissect_request(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    int lenstr;
    uint32_t stmt_id;
    my_stmt_data_t *stmt_data;
    int stmt_pos, param_offset;

    if (conn_data->state == AUTH_SWITCH_RESPONSE)
    {
        LOG_INFO("Request");
        mysql_dissect_auth_switch_response(buf, conn_data);
        return;
    }

    int opcode = buf_readInt8(buf);
    LOG_INFO("Request Opcode 0x%02x %s", opcode, mysql_get_command(opcode, "未知命令"));

    switch (opcode)
    {

    case MYSQL_QUIT:
        break;

    case MYSQL_PROCESS_INFO:
        mysql_set_conn_state(conn_data, RESPONSE_TABULAR);
        break;

    case MYSQL_DEBUG:
    case MYSQL_PING:
        mysql_set_conn_state(conn_data, RESPONSE_OK);
        break;

    case MYSQL_STATISTICS:
        mysql_set_conn_state(conn_data, RESPONSE_MESSAGE);
        break;

    case MYSQL_INIT_DB:
    case MYSQL_CREATE_DB:
    case MYSQL_DROP_DB:
        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Mysql Schema: %s", g_buf);
        mysql_set_conn_state(conn_data, RESPONSE_OK);
        break;

    case MYSQL_QUERY:
        buf_readStr(buf, g_buf, BUFSZ);
        LOG_INFO("Mysql Query: %s", g_buf);
        mysql_set_conn_state(conn_data, RESPONSE_TABULAR);
        break;

    case MYSQL_STMT_PREPARE:
        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Mysql Query: %s", g_buf);
        mysql_set_conn_state(conn_data, RESPONSE_PREPARE);
        break;

    case MYSQL_STMT_CLOSE:
    {
        uint32_t mysql_stmt_id = buf_readInt32LE(buf);
        mysql_set_conn_state(conn_data, REQUEST);
    }
    break;

    case MYSQL_STMT_RESET:
    {
        uint32_t mysql_stmt_id = buf_readInt32LE(buf);
        mysql_set_conn_state(conn_data, RESPONSE_OK);
    }
    break;

    case MYSQL_FIELD_LIST:
        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Mysql Table Name: %s", g_buf);
        mysql_set_conn_state(conn_data, RESPONSE_SHOW_FIELDS);
        break;

    case MYSQL_PROCESS_KILL:
    {
        uint32_t mysql_thd_id = buf_readInt32LE(buf);
    }
        mysql_set_conn_state(conn_data, RESPONSE_OK);
        break;

    case MYSQL_CHANGE_USER:
    {
        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Mysql User %s", g_buf);

        char *mysql_passwd = NULL;
        if (conn_data->clnt_caps & MYSQL_CAPS_SC)
        {
            int len = buf_readInt8(buf);
            buf_readStr(buf, g_buf, len);
        }
        else
        {
            buf_readCStr(buf, g_buf, BUFSZ);
        }
        // LOG_INFO("Mysql Password %s", mysql_passwd);

        buf_readCStr(buf, g_buf, BUFSZ);
        LOG_INFO("Mysql Schema %s", g_buf);

        if (buf_readable(buf))
        {
            uint8_t charset = buf_readInt8(buf);
            buf_retrieve(buf, 1);
            LOG_INFO("Charset [%s](0x%02x)", mysql_get_charset(charset, "未知编码"), charset);
        }
    }
        mysql_set_conn_state(conn_data, RESPONSE_OK);

        char *mysql_client_auth_plugin = NULL;
        /* optional: authentication plugin */
        if (conn_data->clnt_caps_ext & MYSQL_CAPS_PA)
        {
            mysql_set_conn_state(conn_data, AUTH_SWITCH_REQUEST);
            buf_readCStr(buf, g_buf, BUFSZ);
            LOG_INFO("Mysql Client Auth Plugin %s", g_buf);
        }

        /* optional: connection attributes */
        if (conn_data->clnt_caps_ext & MYSQL_CAPS_CA)
        {
            uint64_t lenfle;
            int length;
            uint64_t connattrs_length = buf_readFle(buf, &lenfle, NULL);
            while (connattrs_length > 0)
            {
                int length = mysql_dissect_attributes(buf);
                connattrs_length -= length;
            }
        }
        break;

    case MYSQL_REFRESH:
    {
        uint8_t mysql_refresh = buf_readInt8(buf);
    }
        mysql_set_conn_state(conn_data, RESPONSE_OK);
        break;

    case MYSQL_SHUTDOWN:
    {
        uint8_t mysql_shutdown = buf_readInt8(buf);
    }
        mysql_set_conn_state(conn_data, RESPONSE_OK);
        break;

    case MYSQL_SET_OPTION:
    {
        uint16_t mysql_option = buf_readInt16LE(buf);
    }
        mysql_set_conn_state(conn_data, RESPONSE_OK);
        break;

    case MYSQL_STMT_FETCH:
    {
        uint32_t mysql_stmt_id = buf_readInt32LE(buf);
        uint32_t mysql_num_rows = buf_readInt32LE(buf);
    }
        mysql_set_conn_state(conn_data, RESPONSE_TABULAR);
        break;

    case MYSQL_STMT_SEND_LONG_DATA:
    {
        uint32_t mysql_stmt_id = buf_readInt32LE(buf);
        uint16_t mysql_param = buf_readInt16(buf);
    }

        if (buf_readable(buf))
        {
            buf_readCStr(buf, g_buf, BUFSZ);
            LOG_INFO("Mysql Payload: %s", g_buf); // TODO null str ???
        }
        mysql_set_conn_state(conn_data, REQUEST);
        break;

    case MYSQL_STMT_EXECUTE:
    {
        uint32_t mysql_stmt_id = buf_readInt32LE(buf);
        uint8_t mysql_exec = buf_readInt8(buf);
        uint32_t mysql_exec_iter = buf_readInt32LE(buf);
    }

        // TODO STMT !!!!!
        // stmt_data = (my_stmt_data_t *)wmem_tree_lookup32(conn_data->stmts, stmt_id);
        // if (stmt_data != NULL) {
        // 	if (stmt_data->nparam != 0) {
        // 		uint8_t stmt_bound;
        // 		offset += (stmt_data->nparam + 7) / 8; /* NULL bitmap */
        // 		proto_tree_add_item(req_tree, hf_mysql_new_parameter_bound_flag, tvb, offset, 1, ENC_NA);
        // 		stmt_bound = tvb_get_guint8(tvb, offset);
        // 		offset += 1;
        // 		if (stmt_bound == 1) {
        // 			param_offset = offset + stmt_data->nparam * 2;
        // 			for (stmt_pos = 0; stmt_pos < stmt_data->nparam; stmt_pos++) {
        // 				if (!mysql_dissect_exec_param(req_tree, tvb, &offset, &param_offset,
        // 							      stmt_data->param_flags[stmt_pos], pinfo))
        // 					break;
        // 			}
        // 			offset = param_offset;
        // 		}
        // 	}
        // } else
        {
            if (buf_readable(buf))
            {
                buf_readCStr(buf, g_buf, BUFSZ);
                LOG_INFO("Mysql Payload: %s", g_buf); // TODO null str ???
            }
        }

        /*
		if (buf_readable(buf)) {
			char * mysql_payload = buf_dupStr(buf, buf_readable(buf));
			free(mysql_payload);
		}
        */

        mysql_set_conn_state(conn_data, RESPONSE_TABULAR);
        break;

    case MYSQL_BINLOG_DUMP:
    {
        uint32_t mysql_binlog_position = buf_readInt32LE(buf);
        uint16_t mysql_binlog_flags = buf_readInt16(buf); // BIG_ENDIAN !!!
        uint32_t mysql_binlog_server_id = buf_readInt32LE(buf);
    }

        /* binlog file name ? */
        if (buf_readable(buf))
        {
            buf_readCStr(buf, g_buf, BUFSZ);
            LOG_INFO("Mysql Binlog File Name %s", g_buf);
        }

        mysql_set_conn_state(conn_data, REQUEST);
        break;

    case MYSQL_TABLE_DUMP:
    case MYSQL_CONNECT_OUT:
    case MYSQL_REGISTER_SLAVE:
        mysql_set_conn_state(conn_data, REQUEST);
        break;

    default:
        mysql_set_conn_state(conn_data, UNDEFINED);
    }
}

static void
mysql_dissect_response(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    uint16_t server_status = 0;
    // uint8_t response_code = buf_readInt8(buf);
    uint8_t response_code = buf_peekInt8(buf);

    LOG_INFO("Response Code 0x%02x", response_code);

    if (response_code == 0xff)
    { // ERR
        buf_retrieve(buf, sizeof(uint8_t));
        mysql_dissect_error_packet(buf);
        mysql_set_conn_state(conn_data, REQUEST);
    }
    else if (response_code == 0xfe && buf_readable(buf) < 9)
    { // EOF  !!! < 9
        uint8_t mysql_eof = buf_readInt8(buf);
        LOG_INFO("EOF Marker 0x%02x", mysql_eof);

        /* pre-4.1 packet ends here */
        if (buf_readable(buf))
        {
            uint16_t warn_num = buf_readInt16LE(buf);
            server_status = buf_readInt16LE(buf);
            LOG_INFO("Warnings %d", warn_num);
            LOG_INFO("Server Status 0x%04x", server_status);
        }

        switch (conn_data->state)
        {
        case FIELD_PACKET:
            // 解析完 字段元信息, 继续解析 具体 Row
            mysql_set_conn_state(conn_data, ROW_PACKET);
            break;
        case ROW_PACKET:
            // 解析完 RowPacket 决定继续解析 还是等待 Request
            if (server_status & MYSQL_STAT_MU)
            {
                mysql_set_conn_state(conn_data, RESPONSE_TABULAR);
            }
            else
            {
                mysql_set_conn_state(conn_data, REQUEST);
            }
            break;
        case PREPARED_PARAMETERS:
            if (conn_data->stmt_num_fields > 0)
            {
                mysql_set_conn_state(conn_data, PREPARED_FIELDS);
            }
            else
            {
                mysql_set_conn_state(conn_data, REQUEST);
            }
            break;
        case PREPARED_FIELDS:
            mysql_set_conn_state(conn_data, REQUEST);
            break;
        default:
            /* This should be an unreachable case */
            mysql_set_conn_state(conn_data, REQUEST);
        }
    }
    else if (response_code == 0x00)
    { // OK
        if (conn_data->state == RESPONSE_PREPARE)
        {
            // mysql_dissect_response_prepare(buf, conn_data);
        }
        else if (buf_readable(buf) > buf_peekFleLen(buf))
        {
            mysql_dissect_ok_packet(buf, conn_data);
            if (conn_data->compressed_state == MYSQL_COMPRESS_INIT)
            {
                /* This is the OK packet which follows the compressed protocol setup */
                conn_data->compressed_state = MYSQL_COMPRESS_ACTIVE;
                // TODO
                PANIC("MYSQL_COMPRESS NOT SUPPORT");
            }
        }
        else
        {
            mysql_dissect_result_header(buf, conn_data);
        }
    }
    else
    {
        switch (conn_data->state)
        {
        case RESPONSE_MESSAGE:
            buf_readStr(buf, g_buf, BUFSZ); // 读取所有
            LOG_INFO("Message %s", g_buf);
            mysql_set_conn_state(conn_data, REQUEST);
            break;

        // 处理查询结果集
        case RESPONSE_TABULAR:
            mysql_dissect_result_header(buf, conn_data);
            break;

        case FIELD_PACKET:
        case RESPONSE_SHOW_FIELDS:
        case RESPONSE_PREPARE:
        case PREPARED_PARAMETERS:
            mysql_dissect_field_packet(buf, conn_data);
            break;

        case ROW_PACKET:
            mysql_dissect_row_packet(buf);
            break;

        case PREPARED_FIELDS:
            mysql_dissect_field_packet(buf, conn_data);
            break;

        case AUTH_SWITCH_REQUEST:
            // offset = mysql_dissect_auth_switch_request(tvb, offset, tree, conn_data);
            break;

        default:
            // ti = proto_tree_add_item(tree, hf_mysql_payload, tvb, offset, -1, ENC_NA);
            // expert_add_info(ti, &ei_mysql_unknown_response);
            // offset += tvb_reported_length_remaining(tvb, offset);
            // mysql_set_conn_state(conn_data, UNDEFINED);
            break;
        }
    }
}

static void
mysql_dissect_result_header(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    LOG_INFO("Tabular");
    uint64_t num_fields = buf_readFle(buf, NULL, NULL);
    LOG_INFO("num fields %llu", num_fields);
    if (buf_readable(buf))
    {
        uint64_t extra = buf_readFle(buf, NULL, NULL);
        LOG_INFO("extra %llu", extra);
    }

    if (num_fields)
    {
        mysql_set_conn_state(conn_data, FIELD_PACKET);
    }
    else
    {
        mysql_set_conn_state(conn_data, ROW_PACKET);
    }
}

static void
mysql_dissect_ok_packet(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    LOG_INFO("OK");

    uint64_t affected_rows = buf_readFle(buf, NULL, NULL);
    LOG_INFO("affected rows %llu", affected_rows);

    uint64_t insert_id = buf_readFle(buf, NULL, NULL);
    if (insert_id)
    {
        LOG_INFO("insert id %llu", insert_id);
    }

    uint16_t server_status = 0;
    if (buf_readable(buf))
    {
        server_status = buf_readInt16LE(buf);
        LOG_INFO("Server Status 0x%04x", server_status);

        /* 4.1+ protocol only: 2 bytes number of warnings */
        if (conn_data->clnt_caps & conn_data->srv_caps & MYSQL_CAPS_CU)
        {
            uint16_t warn_num = buf_readInt16LE(buf);
            LOG_INFO("Server Warnings %d", warn_num);
        }
    }

    if (conn_data->clnt_caps_ext & MYSQL_CAPS_ST)
    {
        if (buf_readable(buf))
        {
            int length;

            int lenstr = buf_readFle(buf, NULL, NULL);
            /* first read the optional message */
            if (lenstr)
            {
                buf_readStr(buf, g_buf, lenstr);
                LOG_INFO("Session Track Message %s", g_buf);
            }

            /* session state tracking */
            if (server_status & MYSQL_STAT_SESSION_STATE_CHANGED)
            {
                uint64_t session_track_length = buf_readFle(buf, NULL, NULL);
                LOG_INFO("Session Track Length %llu", session_track_length);

                while (session_track_length > 0)
                {
                    length = mysql_dissect_session_tracker_entry(buf);
                    session_track_length -= length;
                }
            }
        }
    }
    else
    {
        /* optional: message string */
        if (buf_readable(buf))
        {
            buf_readCStr(buf, g_buf, BUFSZ);
            LOG_INFO("Message %s", g_buf);
        }
    }

    mysql_set_conn_state(conn_data, REQUEST);
}

static void
mysql_dissect_field_packet(struct buffer *buf, mysql_conn_data_t *conn_data)
{
    char *str;
    buf_readFleStr(buf, g_buf, BUFSZ);
    LOG_INFO("Catalog %s", g_buf);

    buf_readFleStr(buf, g_buf, BUFSZ);
    LOG_INFO("Database %s", g_buf);

    buf_readFleStr(buf, g_buf, BUFSZ);
    LOG_INFO("Table %s", g_buf);

    buf_readFleStr(buf, g_buf, BUFSZ);
    LOG_INFO("Original Table %s", g_buf);

    buf_readFleStr(buf, g_buf, BUFSZ);
    LOG_INFO("Name %s", g_buf);

    buf_readFleStr(buf, g_buf, BUFSZ);
    LOG_INFO("Orginal Name %s", g_buf);

    buf_retrieve(buf, 1);

    uint16_t charset = buf_readInt16LE(buf);
    uint32_t length = buf_readInt32LE(buf);
    uint8_t type = buf_readInt8(buf);
    uint16_t flags = buf_readInt16LE(buf);
    uint8_t decimal = buf_readInt8(buf);
    LOG_INFO("Charset [%s](0x%02x)", mysql_get_charset(charset, "未知编码"), charset);
    LOG_INFO("Length %d", length);
    LOG_INFO("Type [%s](%d)", mysql_get_field_type(type, "未知类型"), type);
    LOG_INFO("Flags 0x%04x", flags);
    LOG_INFO("Decimal %d", decimal);

    buf_retrieve(buf, 2);

    /* default (Only use for show fields) */
    if (buf_readable(buf))
    {
        buf_readFleStr(buf, g_buf, BUFSZ);
        LOG_INFO("Default %s", g_buf);
    }
}

/*
  Add a session track entry to the session tracking subtree

  return bytes read
*/
static int
mysql_dissect_session_tracker_entry(struct buffer *buf)
{

    uint64_t lenstr;
    uint64_t lenfle;

    /* session tracker type */
    uint8_t data_type = buf_readInt8(buf);
    uint64_t length = buf_readFle(buf, &lenfle, NULL); /* complete length of session tracking entry */
    int sz = 1 + lenfle + length;

    switch (data_type)
    {
    case 0: /* SESSION_SYSVARS_TRACKER */
        lenstr = buf_readFle(buf, &lenfle, NULL);
        buf_readStr(buf, g_buf, lenstr);
        LOG_INFO("Session Track Sysvar Name %s", g_buf);

        lenstr = buf_readFle(buf, &lenfle, NULL);
        buf_readStr(buf, g_buf, lenstr);
        LOG_INFO("Session Track Sysvar Value %s", g_buf);
        break;
    case 1: /* CURRENT_SCHEMA_TRACKER */
        lenstr = buf_readFle(buf, &lenfle, NULL);
        buf_readStr(buf, g_buf, lenstr);
        LOG_INFO("Session Track Sysvar Schema %s", g_buf);
        break;
    case 2: /* SESSION_STATE_CHANGE_TRACKER */
        LOG_INFO("Session Track State Change");
        break;
    default: /* unsupported types skipped */
        LOG_INFO("UnSupported Session Track Types");
    }

    return sz;
}

static void
mysql_dissect_row_packet(struct buffer *buf)
{
    while (buf_readable(buf))
    {
        uint8_t is_null;
        uint64_t lelen = buf_readFle(buf, NULL, &is_null);
        if (is_null)
        {
            LOG_INFO("NULL");
        }
        else
        {
            buf_readStr(buf, g_buf, lelen);
            LOG_INFO("Text: %s", g_buf);
        }
    }
}