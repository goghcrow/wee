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
#include "../base/log.h"

#if !defined(UNUSED)
#define UNUSED(x) ((void)(x))
#endif

// 每个端口挂一个 struct conn(单向连接) 链表
#define PORT_QUEUE_SIZE 65535
static QUEUE PORT_QUEUE[PORT_QUEUE_SIZE];

static int16_t mysql_server_port;
static struct mysql_conn_data g_conn_data;

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

static uint64_t buf_readFLE(struct buffer* buf, uint64_t *len, uint8_t *is_null);
static int buf_peekFLELen(struct buffer* buf);
static int buf_readFLEStr(struct buffer* buf, char** str);

static void mysql_dissect_error_packet(struct buffer *buf);
static void mysql_set_conn_state(mysql_conn_data_t *conn_data, mysql_state_t state);
static void mysql_dissect_greeting(struct buffer *buf, mysql_conn_data_t *conn_data);
static void mysql_dissect_login(struct buffer *buf, mysql_conn_data_t *conn_data);
static int add_connattrs_entry_to_tree(struct buffer *buf);


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

    static char s_ip_buf[INET_ADDRSTRLEN];
    static char d_ip_buf[INET_ADDRSTRLEN];

    // uint32_t d_ip = ip_hdr->ip_dst.s_addr;
    uint16_t d_port = ntohs(tcp_hdr->th_dport);

    inet_ntop(AF_INET, &(ip_hdr->ip_src.s_addr), s_ip_buf, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip_hdr->ip_dst.s_addr), d_ip_buf, INET_ADDRSTRLEN);

    // 连接关闭, 清理数据
    if (tcp_hdr->th_flags & TH_FIN || tcp_hdr->th_flags & TH_RST)
    {
        LOG_INFO("%s:%d > %s:%d Close Connection\n", s_ip_buf, s_port, d_ip_buf, d_port);
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

    if (!is_completed_mysql_pdu(buf))
    {
        return;
    }

    bool is_response = false;
    if (s_port == mysql_server_port)
    {
        is_response = true;
    }

    int32_t pkt_sz = buf_readInt32LE24(buf);
    uint8_t pkt_num = buf_readInt8(buf);


    // TODO 照顾逻辑 !!!!!copy 一份数据包, 存入一个新的buffer !!!!!
    // 优化, buffer_view slice 一个 只读buffer
    struct buffer * once_buf = buf_create(pkt_sz);
    buf_append(once_buf, buf_peek(buf), pkt_sz);
    buf_retrieve(buf, pkt_sz);

    // LOG_INFO("packet size %d packet num %d\n", pkt_sz, pkt_num);
    // TODO 检测是否是 SSL !!!
    bool is_ssl = false;

    if (is_response)
    {
        if (pkt_num == 0 && g_conn_data.state == UNDEFINED)
        {
            LOG_INFO("%s:%d > %s:%d Server Greeting\n", s_ip_buf, s_port, d_ip_buf, s_port);
            mysql_dissect_greeting(once_buf, &g_conn_data);
        }
        else
        {
            LOG_INFO("%s:%d > %s:%d Response\n", s_ip_buf, s_port, d_ip_buf, s_port);
            // mysql_dissect_response
        }
    }
    else
    {
        if (g_conn_data.state == LOGIN && (pkt_num == 1 || (pkt_num == 2 && is_ssl)))
        {
            LOG_INFO("%s:%d > %s:%d Login Request\n", s_ip_buf, s_port, d_ip_buf, s_port);
            mysql_dissect_login(once_buf, &g_conn_data);
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
            LOG_INFO("%s:%d > %s:%d Request\n", s_ip_buf, s_port, d_ip_buf, s_port);
            // mysql_dissect_request
        }
    }

    if (buf_internalCapacity(buf) > 1024 * 1024)
    {
        buf_shrink(c->buf, 0);
    }
    buf_release(once_buf);
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

    init();

    if (!tcpsniff(&opt, pkt_handle))
    {
        fprintf(stderr, "fail to sniff\n");
    }
    return 0;
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
buf_readFLE(struct buffer* buf, uint64_t *len, uint8_t *is_null)
{
	uint8_t prefix = buf_readInt8(buf);
	
	if (is_null) {
		*is_null = 0;
	}

	switch (prefix) {
	case 251:
		if (len) {
			*len = 1;
		}
		if (is_null) {
			*is_null = 1;
		}
		return 0;
	case 252:
		if (len) {
			*len = 1 + 2;
		}
		return buf_readInt16LE(buf);
	case 253:
		if (len) {
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
		if (len) {
			*len = 1 + 8;
		}
		return buf_readInt64LE(buf);
	default: /* < 251 */
		if (len) {
			*len = 1;
		}
		return prefix;
	}
}

static int
buf_peekFLELen(struct buffer* buf)
{
	uint8_t prefix = buf_readInt8(buf);
	
	switch (prefix) {
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


// TODO free
static int
buf_readFLEStr(struct buffer* buf, char** str) {
	uint64_t len;
	uint64_t sz = buf_readFLE(buf, &len, NULL);
	*str = buf_readStr(buf, sz);
	return len + sz;
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
    int protocol = buf_readInt8(buf);
    if (protocol == 0xff)
    {
        mysql_dissect_error_packet(buf);
        return;
    }

    // TODO !!!!! protocol ?????
    // assert(protocol == 0x00);
    mysql_set_conn_state(conn_data, LOGIN);

    // null 结尾字符串, Banner
    // TODO free
    char *ver_str = buf_readCStr(buf);
    LOG_INFO("Mysql Server Version: %s\n", ver_str);
    free(ver_str);

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
    LOG_INFO("Server Thread Id %d\n", thread_id);

    /* salt string */
    char *salt = buf_readCStr(buf);
    free(salt);

    /* rest is optional */
    if (!buf_readable(buf))
    {
        return;
    }

    /* 2 bytes CAPS */
    conn_data->srv_caps = buf_readInt16LE(buf);
    /* rest is optional */
    if (!buf_readable(buf))
    {
        return;
    }

    // TODO 打印信息

    /* 1 byte Charset */
    int8_t charset = buf_readInt8(buf);
    /* 2 byte ServerStatus */
    int16_t server_status = buf_readInt16LE(buf);
    /* 2 bytes ExtCAPS */
    conn_data->srv_caps_ext = buf_readInt16LE(buf);
    /* 1 byte Auth Plugin Length */
    int8_t auto_plugin_len = buf_readInt8(buf);
    /* 10 bytes unused */
    buf_retrieve(buf, 10);
    /* 4.1+ server: rest of salt */
    if (buf_readable(buf))
    {
        char *rest_of_salt = buf_readCStr(buf);
        free(rest_of_salt);
    }

    /* 5.x server: auth plugin */
    if (buf_readable(buf))
    {
        char *auth_plugin = buf_readCStr(buf);
        LOG_INFO("Mysql Server Auth Plugin: %s\n", auth_plugin);
        free(auth_plugin);
    }
}

// TODO test
static void
mysql_dissect_error_packet(struct buffer *buf)
{
    int16_t error = buf_readInt16LE(buf);

    char *sqlstate;
    if (*buf_peek(buf) == '#')
    {
        buf_retrieve(buf, 1);

        sqlstate = buf_readStr(buf, 5);
        // TODO free
        free(sqlstate);
    }
    char *errstr = buf_readCStr(buf);
}

static void
mysql_set_conn_state(mysql_conn_data_t *conn_data, mysql_state_t state)
{
    conn_data->state = state;
}

static void
mysql_dissect_login(struct buffer *buf, mysql_conn_data_t *conn_data)
{
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

    /* Next packet will be use SSL */
    if (!(conn_data->frame_start_ssl) && conn_data->clnt_caps & MYSQL_CAPS_SL)
    {
        // col_set_str(pinfo->cinfo, COL_INFO, "Response: SSL Handshake");
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
        /* 4 bytes max package */
        maxpkt = buf_readInt32LE(buf);
        /* 1 byte Charset */
        charset = buf_readInt8(buf);
        /* filler 23 bytes */
        buf_retrieve(buf, 23);
    }
    else
    { /* pre-4.1 */
        /* 3 bytes max package */
        maxpkt = buf_readInt32LE24(buf);
    }

    /* User name */
    char *mysql_user = buf_readCStr(buf);
    LOG_INFO("Client User %s\n", mysql_user);
    free(mysql_user);

    /* rest is optional */
    if (!buf_readable(buf))
    {
        return;
    }

    // TODO mysql_pwd
    char *mysql_pwd;
    /* 两种情况: password: ascii or length+ascii */
    if (conn_data->clnt_caps & MYSQL_CAPS_SC)
    {
        uint8_t lenstr = buf_readInt8(buf);
        mysql_pwd = buf_readStr(buf, lenstr);
    }
    else
    {
        mysql_pwd = buf_readCStr(buf);
    }
    free(mysql_pwd);

    char *mysql_schema = NULL;
    /* optional: initial schema */
    if (conn_data->clnt_caps & MYSQL_CAPS_CD)
    {
        // TODO free
        mysql_schema = buf_readCStr(buf);
        if (mysql_schema == NULL)
        {
            return;
        }
        free(mysql_schema);
    }

    char *mysql_auth_plugin = NULL;
    /* optional: authentication plugin */
    if (conn_data->clnt_caps_ext & MYSQL_CAPS_PA)
    {
        mysql_set_conn_state(conn_data, AUTH_SWITCH_REQUEST);

        mysql_auth_plugin = buf_readCStr(buf);
        LOG_INFO("Client Auth Plugin %s\n", mysql_auth_plugin);
        free(mysql_auth_plugin);
    }

    /* optional: connection attributes */
    if (conn_data->clnt_caps_ext & MYSQL_CAPS_CA && buf_readable(buf))
    {
        uint64_t connattrs_length = buf_readFLE(buf, NULL, NULL);
        while (connattrs_length > 0)
        {
            int length = add_connattrs_entry_to_tree(buf);
            connattrs_length -= length;
        }
    }
}

static int
add_connattrs_entry_to_tree(struct buffer *buf) {
	int lenfle;

	char* mysql_connattrs_name = NULL;
	char* mysql_connattrs_value = NULL;
	int name_len = buf_readFLEStr(buf, &mysql_connattrs_name);
	int val_len = buf_readFLEStr(buf, &mysql_connattrs_value);
    LOG_INFO("Client Attributes %s %s\n", mysql_connattrs_name, mysql_connattrs_value);
	free(mysql_connattrs_name);
	free(mysql_connattrs_value);
	return name_len + val_len;
}