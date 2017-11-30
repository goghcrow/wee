#ifndef MYSQL_SNIFF_H
#define MYSQL_SNIFF_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define LOG_INFO(fmt, ...) \
    fprintf(stderr, "\x1B[1;32m" fmt "\x1B[0m\n", ##__VA_ARGS__);

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "\x1B[1;31m" fmt "\x1B[0m\n", ##__VA_ARGS__);

#define PANIC(fmt, ...)                                                                                                     \
    fprintf(stderr, "\x1B[1;31m" fmt "\x1B[0m in function %s %s:%d\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__); \
    exit(1);



#define MYSQL_MAX_PACKET_LEN  	0xFFFFFF
#define MYSQL_ERRMSG_SIZE 		512
#define MYSQL_DEFAULT_CATEGORY 	"def"

/* client/server capabilities
 * Source: http://dev.mysql.com/doc/internals/en/capability-flags.html
 * Source: mysql_com.h
 */
#define MYSQL_CAPS_LP 0x0001 /* CLIENT_LONG_PASSWORD */
#define MYSQL_CAPS_FR 0x0002 /* CLIENT_FOUND_ROWS */
#define MYSQL_CAPS_LF 0x0004 /* CLIENT_LONG_FLAG */
#define MYSQL_CAPS_CD 0x0008 /* CLIENT_CONNECT_WITH_DB */
#define MYSQL_CAPS_NS 0x0010 /* CLIENT_NO_SCHEMA */
#define MYSQL_CAPS_CP 0x0020 /* CLIENT_COMPRESS */
#define MYSQL_CAPS_OB 0x0040 /* CLIENT_ODBC */
#define MYSQL_CAPS_LI 0x0080 /* CLIENT_LOCAL_FILES */
#define MYSQL_CAPS_IS 0x0100 /* CLIENT_IGNORE_SPACE */
#define MYSQL_CAPS_CU 0x0200 /* CLIENT_PROTOCOL_41 */
#define MYSQL_CAPS_IA 0x0400 /* CLIENT_INTERACTIVE */
#define MYSQL_CAPS_SL 0x0800 /* CLIENT_SSL */
#define MYSQL_CAPS_II 0x1000 /* CLIENT_IGNORE_SPACE */
#define MYSQL_CAPS_TA 0x2000 /* CLIENT_TRANSACTIONS */
#define MYSQL_CAPS_RS 0x4000 /* CLIENT_RESERVED */
#define MYSQL_CAPS_SC 0x8000 /* CLIENT_SECURE_CONNECTION */


/* field flags */
#define MYSQL_FLD_NOT_NULL_FLAG       0x0001
#define MYSQL_FLD_PRI_KEY_FLAG        0x0002
#define MYSQL_FLD_UNIQUE_KEY_FLAG     0x0004
#define MYSQL_FLD_MULTIPLE_KEY_FLAG   0x0008
#define MYSQL_FLD_BLOB_FLAG           0x0010
#define MYSQL_FLD_UNSIGNED_FLAG       0x0020
#define MYSQL_FLD_ZEROFILL_FLAG       0x0040
#define MYSQL_FLD_BINARY_FLAG         0x0080
#define MYSQL_FLD_ENUM_FLAG           0x0100
#define MYSQL_FLD_AUTO_INCREMENT_FLAG 0x0200
#define MYSQL_FLD_TIMESTAMP_FLAG      0x0400
#define MYSQL_FLD_SET_FLAG            0x0800

/* extended capabilities: 4.1+ client only
 *
 * These are libmysqlclient flags and NOT present
 * in the protocol:
 * CLIENT_SSL_VERIFY_SERVER_CERT (1UL << 30)
 * CLIENT_REMEMBER_OPTIONS (1UL << 31)
 */
#define MYSQL_CAPS_MS 0x0001 /* CLIENT_MULTI_STATMENTS */
#define MYSQL_CAPS_MR 0x0002 /* CLIENT_MULTI_RESULTS */
#define MYSQL_CAPS_PM 0x0004 /* CLIENT_PS_MULTI_RESULTS */
#define MYSQL_CAPS_PA 0x0008 /* CLIENT_PLUGIN_AUTH */
#define MYSQL_CAPS_CA 0x0010 /* CLIENT_CONNECT_ATTRS */
#define MYSQL_CAPS_AL 0x0020 /* CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA */
#define MYSQL_CAPS_EP 0x0040 /* CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS */
#define MYSQL_CAPS_ST 0x0080 /* CLIENT_SESSION_TRACK */
#define MYSQL_CAPS_DE 0x0100 /* CLIENT_DEPRECATE_EOF */
#define MYSQL_CAPS_UNUSED 0xFE00

/* status bitfield */
#define MYSQL_STAT_IT 0x0001
#define MYSQL_STAT_AC 0x0002
#define MYSQL_STAT_MR 0x0004
#define MYSQL_STAT_MU 0x0008
#define MYSQL_STAT_BI 0x0010
#define MYSQL_STAT_NI 0x0020
#define MYSQL_STAT_CR 0x0040
#define MYSQL_STAT_LR 0x0080
#define MYSQL_STAT_DR 0x0100
#define MYSQL_STAT_BS 0x0200
#define MYSQL_STAT_SESSION_STATE_CHANGED 0x0400
#define MYSQL_STAT_QUERY_WAS_SLOW 0x0800
#define MYSQL_STAT_PS_OUT_PARAMS 0x1000

/* bitfield for MYSQL_REFRESH */
#define MYSQL_RFSH_GRANT   1   /* Refresh grant tables */
#define MYSQL_RFSH_LOG     2   /* Start on new log file */
#define MYSQL_RFSH_TABLES  4   /* close all tables */
#define MYSQL_RFSH_HOSTS   8   /* Flush host cache */
#define MYSQL_RFSH_STATUS  16  /* Flush status variables */
#define MYSQL_RFSH_THREADS 32  /* Flush thread cache */
#define MYSQL_RFSH_SLAVE   64  /* Reset master info and restart slave thread */
#define MYSQL_RFSH_MASTER  128 /* Remove all bin logs in the index and truncate the index */

/* MySQL command codes */
#define MYSQL_SLEEP               0  /* not from client */
#define MYSQL_QUIT                1
#define MYSQL_INIT_DB             2
#define MYSQL_QUERY               3
#define MYSQL_FIELD_LIST          4
#define MYSQL_CREATE_DB           5
#define MYSQL_DROP_DB             6
#define MYSQL_REFRESH             7
#define MYSQL_SHUTDOWN            8
#define MYSQL_STATISTICS          9
#define MYSQL_PROCESS_INFO        10
#define MYSQL_CONNECT             11 /* not from client */
#define MYSQL_PROCESS_KILL        12
#define MYSQL_DEBUG               13
#define MYSQL_PING                14
#define MYSQL_TIME                15 /* not from client */
#define MYSQL_DELAY_INSERT        16 /* not from client */
#define MYSQL_CHANGE_USER         17
#define MYSQL_BINLOG_DUMP         18 /* replication */
#define MYSQL_TABLE_DUMP          19 /* replication */
#define MYSQL_CONNECT_OUT         20 /* replication */
#define MYSQL_REGISTER_SLAVE      21 /* replication */
#define MYSQL_STMT_PREPARE        22
#define MYSQL_STMT_EXECUTE        23
#define MYSQL_STMT_SEND_LONG_DATA 24
#define MYSQL_STMT_CLOSE          25
#define MYSQL_STMT_RESET          26
#define MYSQL_SET_OPTION          27
#define MYSQL_STMT_FETCH          28

/* MySQL cursor types */

#define MYSQL_CURSOR_TYPE_NO_CURSOR  0
#define MYSQL_CURSOR_TYPE_READ_ONLY  1
#define MYSQL_CURSOR_TYPE_FOR_UPDATE 2
#define MYSQL_CURSOR_TYPE_SCROLLABLE 4

/* MySQL parameter flags -- used internally by the dissector */

#define MYSQL_PARAM_FLAG_STREAMED 0x01

/* Compression states, internal to the dissector */
#define MYSQL_COMPRESS_NONE   0
#define MYSQL_COMPRESS_INIT   1
#define MYSQL_COMPRESS_ACTIVE 2

struct val_str {
    uint32_t      val;
    const char	  *str;
};

/* decoding table: command */
const struct val_str mysql_command_vals[] = {
	{MYSQL_SLEEP,   "SLEEP"},
	{MYSQL_QUIT,   "Quit"},
	{MYSQL_INIT_DB,  "Use Database"},
	{MYSQL_QUERY,   "Query"},
	{MYSQL_FIELD_LIST, "Show Fields"},
	{MYSQL_CREATE_DB,  "Create Database"},
	{MYSQL_DROP_DB , "Drop Database"},
	{MYSQL_REFRESH , "Refresh"},
	{MYSQL_SHUTDOWN , "Shutdown"},
	{MYSQL_STATISTICS , "Statistics"},
	{MYSQL_PROCESS_INFO , "Process List"},
	{MYSQL_CONNECT , "Connect"},
	{MYSQL_PROCESS_KILL , "Kill Server Thread"},
	{MYSQL_DEBUG , "Dump Debuginfo"},
	{MYSQL_PING , "Ping"},
	{MYSQL_TIME , "Time"},
	{MYSQL_DELAY_INSERT , "Insert Delayed"},
	{MYSQL_CHANGE_USER , "Change User"},
	{MYSQL_BINLOG_DUMP , "Send Binlog"},
	{MYSQL_TABLE_DUMP, "Send Table"},
	{MYSQL_CONNECT_OUT, "Slave Connect"},
	{MYSQL_REGISTER_SLAVE, "Register Slave"},
	{MYSQL_STMT_PREPARE, "Prepare Statement"},
	{MYSQL_STMT_EXECUTE, "Execute Statement"},
	{MYSQL_STMT_SEND_LONG_DATA, "Send BLOB"},
	{MYSQL_STMT_CLOSE, "Close Statement"},
	{MYSQL_STMT_RESET, "Reset Statement"},
	{MYSQL_SET_OPTION, "Set Option"},
	{MYSQL_STMT_FETCH, "Fetch Data"},
	{0, NULL}
};

const char* val_to_str(uint32_t val, char *def)
{
	struct val_str *p = (struct val_str *)mysql_command_vals - 1;
	while((++p)->str) {
		if (p->val == val) {
			return p->str;
		}
	}
	return def;
}



typedef enum mysql_state {
	UNDEFINED,
	LOGIN,
	REQUEST,
	RESPONSE_OK,
	RESPONSE_MESSAGE,
	RESPONSE_TABULAR,
	RESPONSE_SHOW_FIELDS,
	FIELD_PACKET,
	ROW_PACKET,
	RESPONSE_PREPARE,
	PREPARED_PARAMETERS,
	PREPARED_FIELDS,
	AUTH_SWITCH_REQUEST,
	AUTH_SWITCH_RESPONSE
} mysql_state_t;

typedef struct mysql_conn_data {	
    uint16_t srv_caps;
	uint16_t srv_caps_ext;
	uint16_t clnt_caps;
	uint16_t clnt_caps_ext;
	mysql_state_t state;
	uint16_t stmt_num_params;
	uint16_t stmt_num_fields;
	// wmem_tree_t* stmts;
#ifdef CTDEBUG
	uint32_t generation;
#endif
	uint8_t major_version;
	uint32_t frame_start_ssl;
	uint32_t frame_start_compressed;
	uint8_t compressed_state;
} mysql_conn_data_t;

struct mysql_frame_data {
	mysql_state_t state;
};

typedef struct my_stmt_data {
	uint16_t nparam;
	uint8_t* param_flags;
} my_stmt_data_t;

typedef struct mysql_exec_dissector {
	uint8_t type;
	uint8_t unsigned_flag;
    void (*dissector)(struct buffer *buf)
} mysql_exec_dissector_t;

#endif