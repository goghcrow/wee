#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "mysql_sniff.h"
#include "../base/buffer.h"
#include "../base/endian.h"

/* dissector configuration */
static bool mysql_desegment = true;
static bool mysql_showquery = false;


// TODO
// static value_string_ext mysql_command_vals_ext = VALUE_STRING_EXT_INIT(mysql_command_vals);

/* decoding table: exec_flags */
static const value_string mysql_exec_flags_vals[] = {
	{MYSQL_CURSOR_TYPE_NO_CURSOR, "Defaults"},
	{MYSQL_CURSOR_TYPE_READ_ONLY, "Read-only cursor"},
	{MYSQL_CURSOR_TYPE_FOR_UPDATE, "Cursor for update"},
	{MYSQL_CURSOR_TYPE_SCROLLABLE, "Scrollable cursor"},
	{0, NULL}
};

/* decoding table: new_parameter_bound_flag */
static const value_string mysql_new_parameter_bound_flag_vals[] = {
	{0, "Subsequent call"},
	{1, "First call or rebound"},
	{0, NULL}
};

/* decoding table: exec_time_sign */
static const value_string mysql_exec_time_sign_vals[] = {
	{0, "Positive"},
	{1, "Negative"},
	{0, NULL}
};



// TOOD
// static value_string_ext mysql_collation_vals_ext = VALUE_STRING_EXT_INIT(mysql_collation_vals);


/* allowed MYSQL_SHUTDOWN levels */
static const value_string mysql_shutdown_vals[] = {
	{0,   "default"},
	{1,   "wait for connections to finish"},
	{2,   "wait for transactions to finish"},
	{8,   "wait for updates to finish"},
	{16,  "wait flush all buffers"},
	{17,  "wait flush critical buffers"},
	{254, "kill running queries"},
	{255, "kill connections"},
	{0, NULL}
};


/* allowed MYSQL_SET_OPTION values */
static const value_string mysql_option_vals[] = {
	{0, "multi statements on"},
	{1, "multi statements off"},
	{0, NULL}
};

static const value_string mysql_session_track_type_vals[] = {
	{0, "SESSION_SYSVARS_TRACKER"},
	{1, "CURRENT_SCHEMA_TRACKER"},
	{2, "SESSION_STATE_CHANGE_TRACKER"},
	{0, NULL}
};






// static const mysql_exec_dissector_t mysql_exec_dissectors[] = {
// 	{ 0x01, 0, mysql_dissect_exec_tiny },
// 	{ 0x02, 0, mysql_dissect_exec_short },
// 	{ 0x03, 0, mysql_dissect_exec_long },
// 	{ 0x04, 0, mysql_dissect_exec_float },
// 	{ 0x05, 0, mysql_dissect_exec_double },
// 	{ 0x06, 0, mysql_dissect_exec_null },
// 	{ 0x07, 0, mysql_dissect_exec_datetime },
// 	{ 0x08, 0, mysql_dissect_exec_longlong },
// 	{ 0x0a, 0, mysql_dissect_exec_datetime },
// 	{ 0x0b, 0, mysql_dissect_exec_time },
// 	{ 0x0c, 0, mysql_dissect_exec_datetime },
// 	{ 0xf6, 0, mysql_dissect_exec_string },
// 	{ 0xfc, 0, mysql_dissect_exec_string },
// 	{ 0xfd, 0, mysql_dissect_exec_string },
// 	{ 0xfe, 0, mysql_dissect_exec_string },
// 	{ 0x00, 0, NULL },
// };

// length coded binary: a variable-length number
// Length Coded String: a variable-length string.
// Used instead of Null-Terminated String,
// especially for character strings which might contain '/0' or might be very long.
// The first part of a Length Coded String is a Length Coded Binary number (the length);
// the second part of a Length Coded String is the actual data. An example of a short
// Length Coded String is these three hexadecimal bytes: 02 61 62, which means "length = 2, contents = 'ab'".









typedef struct _packet_info {
    bool visited;
} packet_info;




struct mysql_err_pkt {
	int16_t errno;
	char* sqlstate;
	char* errstr;
};









static uint32_t
mysql_dissect_exec_string(struct buffer *buf)
{
	uint32_t param_len = buf_readInt8(buf);

	switch (param_len) {
		case 0xfc: /* 252 - 64k chars */
			param_len = buf_readInt16LE(buf);
			break;
		case 0xfd: /* 64k - 16M chars */
			param_len = buf_readInt32LE24(buf);
			break;
		default: /* < 252 chars */
			break;
	}

	return param_len;
}

static void
mysql_dissect_exec_time(struct buffer *buf)
{
	uint8_t param_len = buf_readInt8(buf);
	
	// TODO
	uint8_t mysql_exec_field_time_sign = 0;
	uint32_t mysql_exec_field_time_days = 0;
	uint8_t mysql_exec_field_hour = 0;
	uint8_t mysql_exec_field_minute = 0;
	uint8_t mysql_exec_field_second = 0;
	uint32_t mysql_exec_field_second_b = 0;

	if (param_len >= 1) {
		mysql_exec_field_time_sign = buf_readInt8(buf);
	} else {
		buf_retrieve(buf, 1);
	}
	if (param_len >= 5) {
		mysql_exec_field_time_days = buf_readInt32LE(buf);
	} else {
		buf_retrieve(buf, 4);
	}
	if (param_len >= 8) {
		mysql_exec_field_hour = buf_readInt8(buf);
		mysql_exec_field_minute = buf_readInt8(buf);
		mysql_exec_field_second = buf_readInt8(buf);
	} else {
		buf_retrieve(buf, 3);
	}
	if (param_len >= 12) {
		mysql_exec_field_second_b = buf_readInt32LE(buf);
	} else {
		buf_retrieve(buf, 4);
	}
	
	// 处理掉 > 12 部分
	if (param_len - 12) {
		buf_retrieve(buf, param_len - 12);
	}
}

static void
mysql_dissect_exec_datetime(struct buffer *buf)
{
	uint8_t param_len = buf_readInt8(buf);
	
	uint16_t mysql_exec_field_year = 0;
  	uint8_t mysql_exec_field_month = 0;
    uint8_t mysql_exec_field_day = 0;
	uint8_t mysql_exec_field_hour = 0;
	uint8_t mysql_exec_field_minute = 0;
	uint8_t mysql_exec_field_second = 0;
	uint32_t mysql_exec_field_second_b = 0;

	if (param_len >= 2) {
		mysql_exec_field_year = buf_readInt16LE(buf);
	}
	if (param_len >= 4) {
		mysql_exec_field_month = buf_readInt8(buf);
		mysql_exec_field_day = buf_readInt8(buf);
	}
	if (param_len >= 7) {
		mysql_exec_field_hour = buf_readInt8(buf);
		mysql_exec_field_minute = buf_readInt8(buf);
		mysql_exec_field_second = buf_readInt8(buf);
	}
	if (param_len >= 11) {
		mysql_exec_field_second_b = buf_readInt32LE(buf);
	}
	
	// 处理掉 > 12 部分
	if (param_len - 11) {
		buf_retrieve(buf, param_len - 11);
	}
}

static void
mysql_dissect_exec_tiny(struct buffer *buf)
{
	uint8_t mysql_exec_field_tiny = buf_readInt8(buf);
}

static void
mysql_dissect_exec_short(struct buffer *buf)
{
	uint16_t mysql_exec_field_short = buf_readInt16LE(buf);
}

static void
mysql_dissect_exec_long(struct buffer *buf)
{
	uint32_t mysql_exec_field_long = buf_readInt32LE(buf);
}

static void
mysql_dissect_exec_float(struct buffer *buf)
{
	float mysql_exec_field_float = buf_readInt32LE(buf);
}

static void
mysql_dissect_exec_double(struct buffer *buf)
{
	double mysql_exec_field_double = buf_readInt64LE(buf);
}

static void
mysql_dissect_exec_longlong(struct buffer *buf)
{
	uint64_t mysql_exec_field_longlong = buf_readInt64LE(buf);
}

static void
mysql_dissect_exec_null(struct buffer *buf)
{}

static char
mysql_dissect_exec_param(struct buffer *buf, uint8_t param_flags,packet_info *pinfo)
{
	int dissector_index = 0;

	uint8_t param_type = buf_readInt8(buf);
	uint8_t param_unsigned = buf_readInt8(buf); /* signedness */
	
	if ((param_flags & MYSQL_PARAM_FLAG_STREAMED) == MYSQL_PARAM_FLAG_STREAMED) {
		// TODO
		// expert_add_info(pinfo, field_tree, &ei_mysql_streamed_param);
		return 1;
	}

	// TODO
	// 重构成 switch case ??

	// while (mysql_exec_dissectors[dissector_index].dissector != NULL) {
	// 	if (mysql_exec_dissectors[dissector_index].type == param_type &&
	// 	    mysql_exec_dissectors[dissector_index].unsigned_flag == param_unsigned) {
	// 		mysql_exec_dissectors[dissector_index].dissector(buf);
	// 		return 1;
	// 	}
	// 	dissector_index++;
	// }
	return 0;
}





static void
mysql_dissect_auth_switch_request(struct buffer *buf, packet_info *pinfo, int offset,  mysql_conn_data_t *conn_data)
{
	// col_set_str(pinfo->cinfo, COL_INFO, "Auth Switch Request" );
	mysql_set_conn_state(pinfo, conn_data, AUTH_SWITCH_RESPONSE);

	/* Status (Always 0xfe) */
	uint8_t status = buf_readInt8(buf);
	assert(status == 0xfe);
	
	/* name */
	// TODO free
	char *name = buf_readCStr(buf);
	free(name);

	/* Data */
	// TODO free
	char *data = buf_readCStr(buf);
	free(data);

	// TODO 消费掉当前 package 剩余数据
	// buf_retrieveAll(buf);
}



static int
mysql_dissect_response_prepare(struct buffer *buf, packet_info *pinfo, mysql_conn_data_t *conn_data)
{
	/* 0, marker for OK packet */
	// TODO ??
	buf_retrieve(buf, 1);
	
	uint32_t mysql_stmt_id = buf_readInt32LE(buf);
	conn_data->stmt_num_fields = buf_readInt16LE(buf);
	conn_data->stmt_num_params = buf_readInt16LE(buf);

	// // TODO free
	// my_stmt_data_t *stmt_data = malloc(sizeof(* stmt_data));
	// assert(stmt_data);
	// stmt_data->nparam = conn_data->stmt_num_params;
	// int flagsize = (int)(sizeof(uint8_t) * stmt_data->nparam);
	// // TODO free
	// stmt_data->param_flags = (uint8_t *)malloc(flagsize);
	// assert(stmt_data->param_flags);
	// memset(stmt_data->param_flags, 0, flagsize);

	// // TODO 这里采用 hash 表 来保存 stmt
	// // wmem_tree_insert32(conn_data->stmts, stmt_id, stmt_data);


	/* Filler */
	// TODO ??
	buf_retrieve(buf, 1);

	uint16_t mysql_num_warn = buf_readInt16LE(buf);

	if (conn_data->stmt_num_params > 0) {
		mysql_set_conn_state(pinfo, conn_data, PREPARED_PARAMETERS);
	} else if (conn_data->stmt_num_fields > 0) {
		mysql_set_conn_state(pinfo, conn_data, PREPARED_FIELDS);
	} else {
		mysql_set_conn_state(pinfo, conn_data, REQUEST);
	}

	// 消费掉剩余数据
	// buf_retrieveAll(buf);
}