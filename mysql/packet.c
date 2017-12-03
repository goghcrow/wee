#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "mysql_sniff.h"
#include "../base/buffer.h"
#include "../base/endian.h"


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