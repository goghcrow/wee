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



/* Initializes an extended value string. Behaves like a match function to
 * permit lazy initialization of extended value strings.
 * - Goes through the value_string array to determine the fastest possible
 *   access method.
 * - Verifies that the value_string contains no NULL string pointers.
 * - Verifies that the value_string is terminated by {0, NULL}
 */
// const value_string *
// _try_val_to_str_ext_init(const guint32 val, value_string_ext *vse)
// {
//     const value_string *vs_p           = vse->_vs_p;
//     const guint         vs_num_entries = vse->_vs_num_entries;

//     /* The matching algorithm used:
//      * VS_SEARCH   - slow sequential search (as in a normal value string)
//      * VS_BIN_TREE - log(n)-time binary search, the values must be sorted
//      * VS_INDEX    - constant-time index lookup, the values must be contiguous
//      */
//     enum { VS_SEARCH, VS_BIN_TREE, VS_INDEX } type = VS_INDEX;

//     /* Note: The value_string 'value' is *unsigned*, but we do a little magic
//      * to help with value strings that have negative values.
//      *
//      * { -3, -2, -1, 0, 1, 2 }
//      * will be treated as "ascending ordered" (although it isn't technically),
//      * thus allowing constant-time index search
//      *
//      * { -3, -2, 0, 1, 2 } and { -3, -2, -1, 0, 2 }
//      * will both be considered as "out-of-order with gaps", thus falling
//      * back to the slow linear search
//      *
//      * { 0, 1, 2, -3, -2 } and { 0, 2, -3, -2, -1 }
//      * will be considered "ascending ordered with gaps" thus allowing
//      * a log(n)-time 'binary' search
//      *
//      * If you're confused, think of how negative values are represented, or
//      * google two's complement.
//      */

//     guint32 prev_value;
//     guint   first_value;
//     guint   i;

//     DISSECTOR_ASSERT((vs_p[vs_num_entries].value  == 0) &&
//                      (vs_p[vs_num_entries].strptr == NULL));

//     vse->_vs_first_value = vs_p[0].value;
//     first_value          = vs_p[0].value;
//     prev_value           = first_value;

//     for (i = 0; i < vs_num_entries; i++) {
//         DISSECTOR_ASSERT(vs_p[i].strptr != NULL);
//         if ((type == VS_INDEX) && (vs_p[i].value != (i + first_value))) {
//             type = VS_BIN_TREE;
//         }
//         /* XXX: Should check for dups ?? */
//         if (type == VS_BIN_TREE) {
//             if (prev_value > vs_p[i].value) {
//                 ws_g_warning("Extended value string '%s' forced to fall back to linear search:\n"
//                           "  entry %u, value %u [%#x] < previous entry, value %u [%#x]",
//                           vse->_vs_name, i, vs_p[i].value, vs_p[i].value, prev_value, prev_value);
//                 type = VS_SEARCH;
//                 break;
//             }
//             if (first_value > vs_p[i].value) {
//                 ws_g_warning("Extended value string '%s' forced to fall back to linear search:\n"
//                           "  entry %u, value %u [%#x] < first entry, value %u [%#x]",
//                           vse->_vs_name, i, vs_p[i].value, vs_p[i].value, first_value, first_value);
//                 type = VS_SEARCH;
//                 break;
//             }
//         }

//         prev_value = vs_p[i].value;
//     }

//     switch (type) {
//         case VS_SEARCH:
//             vse->_vs_match2 = _try_val_to_str_linear;
//             break;
//         case VS_BIN_TREE:
//             vse->_vs_match2 = _try_val_to_str_bsearch;
//             break;
//         case VS_INDEX:
//             vse->_vs_match2 = _try_val_to_str_index;
//             break;
//         default:
//             g_assert_not_reached();
//             break;
//     }

//     return vse->_vs_match2(val, vse);
// }



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


/* collation codes may change over time, recreate with the following SQL

SELECT CONCAT('  {', ID, ',"', CHARACTER_SET_NAME, ' COLLATE ', COLLATION_NAME, '"},')
FROM INFORMATION_SCHEMA.COLLATIONS
ORDER BY ID
INTO OUTFILE '/tmp/mysql-collations';

*/
static const value_string mysql_collation_vals[] = {
	{3,   "dec8 COLLATE dec8_swedish_ci"},
	{4,   "cp850 COLLATE cp850_general_ci"},
	{5,   "latin1 COLLATE latin1_german1_ci"},
	{6,   "hp8 COLLATE hp8_english_ci"},
	{7,   "koi8r COLLATE koi8r_general_ci"},
	{8,   "latin1 COLLATE latin1_swedish_ci"},
	{9,   "latin2 COLLATE latin2_general_ci"},
	{10,  "swe7 COLLATE swe7_swedish_ci"},
	{11,  "ascii COLLATE ascii_general_ci"},
	{14,  "cp1251 COLLATE cp1251_bulgarian_ci"},
	{15,  "latin1 COLLATE latin1_danish_ci"},
	{16,  "hebrew COLLATE hebrew_general_ci"},
	{20,  "latin7 COLLATE latin7_estonian_cs"},
	{21,  "latin2 COLLATE latin2_hungarian_ci"},
	{22,  "koi8u COLLATE koi8u_general_ci"},
	{23,  "cp1251 COLLATE cp1251_ukrainian_ci"},
	{25,  "greek COLLATE greek_general_ci"},
	{26,  "cp1250 COLLATE cp1250_general_ci"},
	{27,  "latin2 COLLATE latin2_croatian_ci"},
	{29,  "cp1257 COLLATE cp1257_lithuanian_ci"},
	{30,  "latin5 COLLATE latin5_turkish_ci"},
	{31,  "latin1 COLLATE latin1_german2_ci"},
	{32,  "armscii8 COLLATE armscii8_general_ci"},
	{33,  "utf8 COLLATE utf8_general_ci"},
	{36,  "cp866 COLLATE cp866_general_ci"},
	{37,  "keybcs2 COLLATE keybcs2_general_ci"},
	{38,  "macce COLLATE macce_general_ci"},
	{39,  "macroman COLLATE macroman_general_ci"},
	{40,  "cp852 COLLATE cp852_general_ci"},
	{41,  "latin7 COLLATE latin7_general_ci"},
	{42,  "latin7 COLLATE latin7_general_cs"},
	{43,  "macce COLLATE macce_bin"},
	{44,  "cp1250 COLLATE cp1250_croatian_ci"},
	{45,  "utf8mb4 COLLATE utf8mb4_general_ci"},
	{46,  "utf8mb4 COLLATE utf8mb4_bin"},
	{47,  "latin1 COLLATE latin1_bin"},
	{48,  "latin1 COLLATE latin1_general_ci"},
	{49,  "latin1 COLLATE latin1_general_cs"},
	{50,  "cp1251 COLLATE cp1251_bin"},
	{51,  "cp1251 COLLATE cp1251_general_ci"},
	{52,  "cp1251 COLLATE cp1251_general_cs"},
	{53,  "macroman COLLATE macroman_bin"},
	{57,  "cp1256 COLLATE cp1256_general_ci"},
	{58,  "cp1257 COLLATE cp1257_bin"},
	{59,  "cp1257 COLLATE cp1257_general_ci"},
	{63,  "binary COLLATE binary"},
	{64,  "armscii8 COLLATE armscii8_bin"},
	{65,  "ascii COLLATE ascii_bin"},
	{66,  "cp1250 COLLATE cp1250_bin"},
	{67,  "cp1256 COLLATE cp1256_bin"},
	{68,  "cp866 COLLATE cp866_bin"},
	{69,  "dec8 COLLATE dec8_bin"},
	{70,  "greek COLLATE greek_bin"},
	{71,  "hebrew COLLATE hebrew_bin"},
	{72,  "hp8 COLLATE hp8_bin"},
	{73,  "keybcs2 COLLATE keybcs2_bin"},
	{74,  "koi8r COLLATE koi8r_bin"},
	{75,  "koi8u COLLATE koi8u_bin"},
	{77,  "latin2 COLLATE latin2_bin"},
	{78,  "latin5 COLLATE latin5_bin"},
	{79,  "latin7 COLLATE latin7_bin"},
	{80,  "cp850 COLLATE cp850_bin"},
	{81,  "cp852 COLLATE cp852_bin"},
	{82,  "swe7 COLLATE swe7_bin"},
	{83,  "utf8 COLLATE utf8_bin"},
	{92,  "geostd8 COLLATE geostd8_general_ci"},
	{93,  "geostd8 COLLATE geostd8_bin"},
	{94,  "latin1 COLLATE latin1_spanish_ci"},
	{99,  "cp1250 COLLATE cp1250_polish_ci"},
	{192, "utf8 COLLATE utf8_unicode_ci"},
	{193, "utf8 COLLATE utf8_icelandic_ci"},
	{194, "utf8 COLLATE utf8_latvian_ci"},
	{195, "utf8 COLLATE utf8_romanian_ci"},
	{196, "utf8 COLLATE utf8_slovenian_ci"},
	{197, "utf8 COLLATE utf8_polish_ci"},
	{198, "utf8 COLLATE utf8_estonian_ci"},
	{199, "utf8 COLLATE utf8_spanish_ci"},
	{200, "utf8 COLLATE utf8_swedish_ci"},
	{201, "utf8 COLLATE utf8_turkish_ci"},
	{202, "utf8 COLLATE utf8_czech_ci"},
	{203, "utf8 COLLATE utf8_danish_ci"},
	{204, "utf8 COLLATE utf8_lithuanian_ci"},
	{205, "utf8 COLLATE utf8_slovak_ci"},
	{206, "utf8 COLLATE utf8_spanish2_ci"},
	{207, "utf8 COLLATE utf8_roman_ci"},
	{208, "utf8 COLLATE utf8_persian_ci"},
	{209, "utf8 COLLATE utf8_esperanto_ci"},
	{210, "utf8 COLLATE utf8_hungarian_ci"},
	{211, "utf8 COLLATE utf8_sinhala_ci"},
	{212, "utf8 COLLATE utf8_german2_ci"},
	{213, "utf8 COLLATE utf8_croatian_ci"},
	{214, "utf8 COLLATE utf8_unicode_520_ci"},
	{215, "utf8 COLLATE utf8_vietnamese_ci"},
	{223, "utf8 COLLATE utf8_general_mysql500_ci"},
	{224, "utf8mb4 COLLATE utf8mb4_unicode_ci"},
	{225, "utf8mb4 COLLATE utf8mb4_icelandic_ci"},
	{226, "utf8mb4 COLLATE utf8mb4_latvian_ci"},
	{227, "utf8mb4 COLLATE utf8mb4_romanian_ci"},
	{228, "utf8mb4 COLLATE utf8mb4_slovenian_ci"},
	{229, "utf8mb4 COLLATE utf8mb4_polish_ci"},
	{230, "utf8mb4 COLLATE utf8mb4_estonian_ci"},
	{231, "utf8mb4 COLLATE utf8mb4_spanish_ci"},
	{232, "utf8mb4 COLLATE utf8mb4_swedish_ci"},
	{233, "utf8mb4 COLLATE utf8mb4_turkish_ci"},
	{234, "utf8mb4 COLLATE utf8mb4_czech_ci"},
	{235, "utf8mb4 COLLATE utf8mb4_danish_ci"},
	{236, "utf8mb4 COLLATE utf8mb4_lithuanian_ci"},
	{237, "utf8mb4 COLLATE utf8mb4_slovak_ci"},
	{238, "utf8mb4 COLLATE utf8mb4_spanish2_ci"},
	{239, "utf8mb4 COLLATE utf8mb4_roman_ci"},
	{240, "utf8mb4 COLLATE utf8mb4_persian_ci"},
	{241, "utf8mb4 COLLATE utf8mb4_esperanto_ci"},
	{242, "utf8mb4 COLLATE utf8mb4_hungarian_ci"},
	{243, "utf8mb4 COLLATE utf8mb4_sinhala_ci"},
	{244, "utf8mb4 COLLATE utf8mb4_german2_ci"},
	{245, "utf8mb4 COLLATE utf8mb4_croatian_ci"},
	{246, "utf8mb4 COLLATE utf8mb4_unicode_520_ci"},
	{247, "utf8mb4 COLLATE utf8mb4_vietnamese_ci"},
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



/* type constants */
static const value_string type_constants[] = {
	{0x00, "FIELD_TYPE_DECIMAL"    },
	{0x01, "FIELD_TYPE_TINY"       },
	{0x02, "FIELD_TYPE_SHORT"      },
	{0x03, "FIELD_TYPE_LONG"       },
	{0x04, "FIELD_TYPE_FLOAT"      },
	{0x05, "FIELD_TYPE_DOUBLE"     },
	{0x06, "FIELD_TYPE_NULL"       },
	{0x07, "FIELD_TYPE_TIMESTAMP"  },
	{0x08, "FIELD_TYPE_LONGLONG"   },
	{0x09, "FIELD_TYPE_INT24"      },
	{0x0a, "FIELD_TYPE_DATE"       },
	{0x0b, "FIELD_TYPE_TIME"       },
	{0x0c, "FIELD_TYPE_DATETIME"   },
	{0x0d, "FIELD_TYPE_YEAR"       },
	{0x0e, "FIELD_TYPE_NEWDATE"    },
	{0x0f, "FIELD_TYPE_VARCHAR"    },
	{0x10, "FIELD_TYPE_BIT"        },
	{0xf6, "FIELD_TYPE_NEWDECIMAL" },
	{0xf7, "FIELD_TYPE_ENUM"       },
	{0xf8, "FIELD_TYPE_SET"        },
	{0xf9, "FIELD_TYPE_TINY_BLOB"  },
	{0xfa, "FIELD_TYPE_MEDIUM_BLOB"},
	{0xfb, "FIELD_TYPE_LONG_BLOB"  },
	{0xfc, "FIELD_TYPE_BLOB"       },
	{0xfd, "FIELD_TYPE_VAR_STRING" },
	{0xfe, "FIELD_TYPE_STRING"     },
	{0xff, "FIELD_TYPE_GEOMETRY"   },
	{0, NULL}
};


static const value_string state_vals[] = {
	{UNDEFINED,            "undefined"},
	{LOGIN,                "login"},
	{REQUEST,              "request"},
	{RESPONSE_OK,          "response OK"},
	{RESPONSE_MESSAGE,     "response message"},
	{RESPONSE_TABULAR,     "tabular response"},
	{RESPONSE_SHOW_FIELDS, "response to SHOW FIELDS"},
	{FIELD_PACKET,         "field packet"},
	{ROW_PACKET,           "row packet"},
	{RESPONSE_PREPARE,     "response to PREPARE"},
	{PREPARED_PARAMETERS,  "parameters in response to PREPARE"},
	{PREPARED_FIELDS,      "fields in response to PREPARE"},
	{AUTH_SWITCH_REQUEST,  "authentication switch request"},
	{AUTH_SWITCH_RESPONSE, "authentication switch response"},
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