#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "../base/buffer.h"
#include "../base/endian.h"

/* dissector configuration */
static bool mysql_desegment = true;
static bool mysql_showquery = false;

typedef struct _value_string {
    uint32_t      value;
    const char *strptr;
} value_string;

typedef struct _value_string_ext value_string_ext;
typedef const value_string *(*_value_string_match2_t)(const uint32_t, value_string_ext*);

struct _value_string_ext {
    _value_string_match2_t _vs_match2;
    uint32_t               _vs_first_value; /* first value of the value_string array       */
    uint32_t               _vs_num_entries; /* number of entries in the value_string array */
                                            /*  (excluding final {0, NULL})                */
    const value_string     *_vs_p;          /* the value string array address              */
    const char             *_vs_name;       /* vse "Name" (for error messages)             */
};

const value_string * _try_val_to_str_ext_init(const uint32_t val, value_string_ext *vse);

#define N_ELEMENTS(arr)		(sizeof (arr) / sizeof ((arr)[0]))
// #define VALUE_STRING_EXT_INIT(x) { _try_val_to_str_ext_init, 0, N_ELEMENTS(x)-1, x, #x }


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

/* decoding table: command */
static const value_string mysql_command_vals[] = {
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

static const mysql_exec_dissector_t mysql_exec_dissectors[] = {
	{ 0x01, 0, mysql_dissect_exec_tiny },
	{ 0x02, 0, mysql_dissect_exec_short },
	{ 0x03, 0, mysql_dissect_exec_long },
	{ 0x04, 0, mysql_dissect_exec_float },
	{ 0x05, 0, mysql_dissect_exec_double },
	{ 0x06, 0, mysql_dissect_exec_null },
	{ 0x07, 0, mysql_dissect_exec_datetime },
	{ 0x08, 0, mysql_dissect_exec_longlong },
	{ 0x0a, 0, mysql_dissect_exec_datetime },
	{ 0x0b, 0, mysql_dissect_exec_time },
	{ 0x0c, 0, mysql_dissect_exec_datetime },
	{ 0xf6, 0, mysql_dissect_exec_string },
	{ 0xfc, 0, mysql_dissect_exec_string },
	{ 0xfd, 0, mysql_dissect_exec_string },
	{ 0xfe, 0, mysql_dissect_exec_string },
	{ 0x00, 0, NULL },
};

// length coded binary: a variable-length number
// Length Coded String: a variable-length string.
// Used instead of Null-Terminated String,
// especially for character strings which might contain '/0' or might be very long.
// The first part of a Length Coded String is a Length Coded Binary number (the length);
// the second part of a Length Coded String is the actual data. An example of a short
// Length Coded String is these three hexadecimal bytes: 02 61 62, which means "length = 2, contents = 'ab'".

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
buf_readFLEStr(struct buffer* buf, char* str) {
	int len;
	uint64_t sz = buf_readFLE(buf, &len, NULL);
	str = buf_readStr(buf, sz);
	return len + sz;
}







typedef struct _packet_info {
    bool visited;
} packet_info;


/* Helper function to only set state on first pass */
static void mysql_set_conn_state(packet_info *pinfo, mysql_conn_data_t *conn_data, mysql_state_t state)
{
	if (!pinfo->visited)
	{
		conn_data->state = state;
	}
}



struct mysql_err_pkt {
	int16_t errno;
	char* sqlstate;
	char* errstr;
};

// TODO test
static void
mysql_dissect_error_packet(struct buffer *buf, packet_info *pinfo)
{
	int16_t error = buf_readInt16LE(buf);

	char* sqlstate;
	if (*buf_peek(buf) == '#') {
		buf_retrieve(buf, 1);

		sqlstate = buf_readStr(buf, 5);
		// TODO free
		free(sqlstate);
	}
	char *errstr = buf_readCStr(buf);
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
mysql_dissect_greeting(struct buffer *buf, packet_info *pinfo, mysql_conn_data_t *conn_data)
{
    int protocol = buf_readInt8(buf);
	if (protocol == 0xff) {
		mysql_dissect_error_packet(buf, pinfo);
		return;
	}

	assert(protocol == 0x00);
	mysql_set_conn_state(pinfo, conn_data, LOGIN);
	
	// null 结尾字符串, Banner
	// TODO free
	char* ver_str = buf_readCStr(buf);
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

	/* salt string */
	// TODO free
	char* salt = buf_readCStr(buf);
	free(salt);

	/* rest is optional */
    if (!buf_readable(buf)) {
        return;
    }

	/* 2 bytes CAPS */
	conn_data->srv_caps = buf_readInt16LE(buf);
	/* rest is optional */
    if (!buf_readable(buf)) {
        return;
    }

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
	if (buf_readable(buf)) {
		// TODO free
		char* restOfSalt = buf_readCStr(buf);
		free(restOfSalt);
	}

	/* 5.x server: auth plugin */
	if (buf_readable(buf)) {
		char* auth_plugin = buf_readCStr(buf);
	}
}



static int
add_connattrs_entry_to_tree(struct buffer *buf, packet_info *pinfo) {
	int lenfle;

	char* mysql_connattrs_name;
	char* mysql_connattrs_value;
	int name_len = buf_readFLEStr(buf, mysql_connattrs_name);
	int val_len = buf_readFLEStr(buf, mysql_connattrs_value);
	
	// TODO 保存属性...
	free(mysql_connattrs_name);
	free(mysql_connattrs_value);
	return name_len + val_len;
}


static void
mysql_dissect_login(struct buffer *buf, packet_info *pinfo, mysql_conn_data_t *conn_data)
{
	int lenstr;

	/* after login there can be OK or DENIED */
	mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);

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
	} else { /* pre-4.1 */
		/* 3 bytes max package */
		maxpkt = buf_readInt32LE24(buf);
	}

	/* User name */
	// TODO free
	char* mysql_user = buf_readCStr(buf);
	free(mysql_user);

	/* rest is optional */
	if (!buf_readable(buf)) {
		return;
	}

	// TODO free
	char* mysql_pwd;
	/* 两种情况: password: ascii or length+ascii */
	if (conn_data->clnt_caps & MYSQL_CAPS_SC) {
		uint8_t lenstr = buf_readInt8(buf);
		mysql_pwd = buf_readStr(buf, lenstr);	
	} else {
		mysql_pwd = buf_readCStr(buf);
	}
	free(mysql_pwd);

	char* mysql_schema = NULL;
	/* optional: initial schema */
	if (conn_data->clnt_caps & MYSQL_CAPS_CD)
	{
		// TODO free
		mysql_schema = buf_readCStr(buf);
		if (mysql_schema == NULL) {
			return;
		}
		free(mysql_schema);
	}

	char* mysql_auth_plugin = NULL;
	/* optional: authentication plugin */
	if (conn_data->clnt_caps_ext & MYSQL_CAPS_PA)
	{
		mysql_set_conn_state(pinfo, conn_data, AUTH_SWITCH_REQUEST);
		
		// TODO free
		mysql_auth_plugin = buf_readCStr(buf);
		free(mysql_auth_plugin);
	}

	/* optional: connection attributes */
	if (conn_data->clnt_caps_ext & MYSQL_CAPS_CA && buf_readable(buf))
	{
		uint64_t connattrs_length = buf_readFLE(buf, NULL, NULL);
		// TODO
		while (connattrs_length > 0) {
			int length = add_connattrs_entry_to_tree(buf, pinfo);
			connattrs_length -= length;
		}
	}
}

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

	while (mysql_exec_dissectors[dissector_index].dissector != NULL) {
		if (mysql_exec_dissectors[dissector_index].type == param_type &&
		    mysql_exec_dissectors[dissector_index].unsigned_flag == param_unsigned) {
			mysql_exec_dissectors[dissector_index].dissector(buf);
			return 1;
		}
		dissector_index++;
	}
	return 0;
}

static int
mysql_dissect_request(struct buffer *buf, packet_info *pinfo, 
			mysql_conn_data_t *conn_data, mysql_state_t current_state)
{
	int lenstr;
	// proto_item *request_item, *tf = NULL, *ti;
	// proto_item *req_tree;
	uint32_t stmt_id;
	my_stmt_data_t *stmt_data;
	int stmt_pos, param_offset;

	if(current_state == AUTH_SWITCH_RESPONSE){
		mysql_dissect_auth_switch_response(buf, pinfo, conn_data);
		return;
	}

	int opcode = buf_readInt8(buf);
	// col_append_fstr(pinfo->cinfo, COL_INFO, " %s", val_to_str_ext(opcode, &mysql_command_vals_ext, "Unknown (%u)"));

	switch (opcode) {

	case MYSQL_QUIT:
		break;

	case MYSQL_PROCESS_INFO:
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_TABULAR);
		break;

	case MYSQL_DEBUG:
	case MYSQL_PING:
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);
		break;

	case MYSQL_STATISTICS:
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_MESSAGE);
		break;

	case MYSQL_INIT_DB:
	case MYSQL_CREATE_DB:
	case MYSQL_DROP_DB:
		// TODO
		char * mysql_schema = buf_readCStr(buf);
		free(mysql_schema);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);
		break;

	case MYSQL_QUERY:
		// TODO
		char * mysql_query = buf_readCStr(buf);
		if (mysql_showquery) {
			// TODO
			// col_append_fstr(pinfo->cinfo, COL_INFO, " { %s } ", tvb_format_text(tvb, offset, lenstr));
		}
		free(mysql_query);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_TABULAR);
		break;

	case MYSQL_STMT_PREPARE:
		// TODO
		char * mysql_query = buf_readCStr(buf);
		free(mysql_query);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_PREPARE);
		break;

	case MYSQL_STMT_CLOSE:
		uint32_t mysql_stmt_id = buf_readInt32LE(buf);
		mysql_set_conn_state(pinfo, conn_data, REQUEST);
		break;

	case MYSQL_STMT_RESET:
		uint32_t mysql_stmt_id = buf_readInt32LE(buf);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);
		break;

	case MYSQL_FIELD_LIST:
		// TODO
		char * mysql_table_name = buf_readCStr(buf);
		free(mysql_table_name);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_SHOW_FIELDS);
		break;

	case MYSQL_PROCESS_KILL:
		uint32_t mysql_thd_id = buf_readInt32LE(buf);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);
		break;

	case MYSQL_CHANGE_USER:
		// TODO
		char * mysql_user = buf_readCStr(buf);
		free(mysql_user);

		char * mysql_passwd;
		if (conn_data->clnt_caps & MYSQL_CAPS_SC) {
			int len = buf_readInt8(buf);
			mysql_passwd = buf_readStr(buf, len);
		} else {
			mysql_passwd = buf_readCStr(buf);
		}
		free(mysql_passwd);

		char * mysql_schema = buf_readCStr(buf);
		free(mysql_schema);

		if (buf_readable(buf)) {
			uint8_t charset = buf_readInt8(buf);
			// TODO ????
			buf_retrieve(buf, 1);

		}
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);

		char * mysql_client_auth_plugin = NULL;
		/* optional: authentication plugin */
		if (conn_data->clnt_caps_ext & MYSQL_CAPS_PA)
		{
			mysql_set_conn_state(pinfo, conn_data, AUTH_SWITCH_REQUEST);
			mysql_client_auth_plugin = buf_readCStr(buf);
			free(mysql_client_auth_plugin);
		}

		/* optional: connection attributes */
		if (conn_data->clnt_caps_ext & MYSQL_CAPS_CA)
		{
			int lenfle;
			int length;

			uint64_t connattrs_length = buf_readFLE(buf, &lenfle, NULL);

			while (connattrs_length > 0) {
				int length = add_connattrs_entry_to_tree(buf, pinfo);
				connattrs_length -= length;
			}
		}
		break;

	case MYSQL_REFRESH:
		uint8_t mysql_refresh = buf_readInt8(buf);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);
		break;

	case MYSQL_SHUTDOWN:
		uint8_t mysql_shutdown = buf_readInt8(buf);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);
		break;

	case MYSQL_SET_OPTION:
		uint16_t mysql_option = buf_readInt16LE(buf);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_OK);
		break;

	case MYSQL_STMT_FETCH:
		uint32_t mysql_stmt_id = buf_readInt32LE(buf);
		uint32_t mysql_num_rows = buf_readInt32LE(buf);
		mysql_set_conn_state(pinfo, conn_data, RESPONSE_TABULAR);
		break;

	case MYSQL_STMT_SEND_LONG_DATA:
		uint32_t mysql_stmt_id = buf_readInt32LE(buf);

		// stmt_data = (my_stmt_data_t *)wmem_tree_lookup32(conn_data->stmts, stmt_id);
		// if (stmt_data != NULL) {
		// 	uint16_t data_param = tvb_get_letohs(tvb, offset);
		// 	if (stmt_data->nparam > data_param) {
		// 		stmt_data->param_flags[data_param] |= MYSQL_PARAM_FLAG_STREAMED;
		// 	}
		// }

		uint16_t mysql_param = buf_readInt16(buf);

		/* rest is data */
		char * mysql_payload;
		if (buf_readable(buf)) {
			mysql_payload = buf_readStr(buf, buf_readable(buf));
			free(mysql_payload);
		}
		mysql_set_conn_state(pinfo, conn_data, REQUEST);
		break;

	case MYSQL_STMT_EXECUTE:
		uint32_t mysql_stmt_id = buf_readInt32LE(buf);
		uint8_t mysql_exec = buf_readInt8(buf);
		uint32_t mysql_exec_iter = buf_readInt32LE(buf);

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
		// } else {
			char * mysql_payload;
			if (buf_readable(buf)) {
				mysql_payload = buf_readStr(buf, buf_readable(buf));
				free(mysql_payload);
				// FIXME: execute dissector incomplete
			}
		// }


		char * mysql_payload;
		if (buf_readable(buf)) {
			mysql_payload = buf_readStr(buf, buf_readable(buf));
			free(mysql_payload);
			// FIXME: execute dissector incomplete
		}

		mysql_set_conn_state(pinfo, conn_data, RESPONSE_TABULAR);
		break;

	case MYSQL_BINLOG_DUMP:
		uint32_t mysql_binlog_position = buf_readInt32LE(buf);
		uint16_t mysql_binlog_flags = buf_readInt16(buf); // BIG_ENDIAN !!!
		uint32_t mysql_binlog_server_id = buf_readInt32LE(buf);

		// TODO
		char * mysql_binlog_file_name;
		/* binlog file name ? */
		if (buf_readable(buf)) {
			mysql_binlog_file_name = buf_readStr(buf, buf_readable(buf));
			free(mysql_binlog_file_name);
		}

		mysql_set_conn_state(pinfo, conn_data, REQUEST);
		break;

	/* FIXME: implement replication packets */
	case MYSQL_TABLE_DUMP:
	case MYSQL_CONNECT_OUT:
	case MYSQL_REGISTER_SLAVE:
		// 消费掉当前 packet 剩余数据
		buf_retrieveAll(buf);
		mysql_set_conn_state(pinfo, conn_data, REQUEST);
		break;

	default:
		// 消费掉当前 packet 剩余数据
		buf_retrieveAll(buf);
		mysql_set_conn_state(pinfo, conn_data, UNDEFINED);
	}
}

/*
 * Decode the header of a compressed packet
 * https://dev.mysql.com/doc/internals/en/compressed-packet-header.html
 */
static void
mysql_dissect_compressed_header(struct buffer *buf)
{
 	uint32_t mysql_compressed_packet_length = buf_readInt32LE24(buf);
	uint8_t mysql_compressed_packet_number = buf_readInt8(buf);
	uint32_t mysql_compressed_packet_length_uncompressed = buf_readInt32LE24(buf);
}


// TODO 特么的 offset 都不对!!!!
// 每个 buffer 都需要时一个完整的 mysql pdu
// buf_readable() 都是以上一条为假设的 !!!!!
static int
mysql_dissect_response(struct buffer *buf, packet_info *pinfo, 
				mysql_conn_data_t *conn_data, mysql_state_t current_state)
{

	uint16_t server_status = 0;
	uint8_t response_code = buf_readInt8(buf);
	
	if ( response_code == 0xff ) { // ERR
		mysql_dissect_error_packet(buf, pinfo);
		mysql_set_conn_state(pinfo, conn_data, REQUEST);
	} else if (response_code == 0xfe && buf_readable(buf) < 9) { // EOF
	 	uint8_t mysql_eof = buf_readInt8(buf);

		/* pre-4.1 packet ends here */
		if (buf_readable(buf)) {
			uint16_t mysql_num_warn = buf_readInt16LE(buf);
			server_status = buf_readInt16LE(buf);
		}

		switch (current_state) {
		case FIELD_PACKET:
			mysql_set_conn_state(pinfo, conn_data, ROW_PACKET);
			break;
		case ROW_PACKET:
			if (server_status & MYSQL_STAT_MU) {
				mysql_set_conn_state(pinfo, conn_data, RESPONSE_TABULAR);
			} else {
				mysql_set_conn_state(pinfo, conn_data, REQUEST);
			}
			break;
		case PREPARED_PARAMETERS:
			if (conn_data->stmt_num_fields > 0) {
				mysql_set_conn_state(pinfo, conn_data, PREPARED_FIELDS);
			} else {
				mysql_set_conn_state(pinfo, conn_data, REQUEST);
			}
			break;
		case PREPARED_FIELDS:
			mysql_set_conn_state(pinfo, conn_data, REQUEST);
			break;
		default:
			/* This should be an unreachable case */
			mysql_set_conn_state(pinfo, conn_data, REQUEST);
		}
	} else if (response_code == 0x00) { // OK
		if (current_state == RESPONSE_PREPARE) {
			mysql_dissect_response_prepare(buf, pinfo, conn_data);
		} else if (buf_readable(buf) > buf_peekFLELen(buf)) {
			offset = mysql_dissect_ok_packet(tvb, pinfo, offset+1, tree, conn_data);
			if (conn_data->compressed_state == MYSQL_COMPRESS_INIT) {
				/* This is the OK packet which follows the compressed protocol setup */
				conn_data->compressed_state = MYSQL_COMPRESS_ACTIVE;
			}
		} else {
			offset = mysql_dissect_result_header(tvb, pinfo, offset, tree, conn_data);
		}
	} else {
		switch (current_state) {
		case RESPONSE_MESSAGE:
			if ((lenstr = tvb_reported_length_remaining(tvb, offset))) {
				proto_tree_add_item(tree, hf_mysql_message, tvb, offset, lenstr, ENC_ASCII|ENC_NA);
				offset += lenstr;
			}
			mysql_set_conn_state(pinfo, conn_data, REQUEST);
			break;

		case RESPONSE_TABULAR:
			offset = mysql_dissect_result_header(tvb, pinfo, offset, tree, conn_data);
			break;

		case FIELD_PACKET:
		case RESPONSE_SHOW_FIELDS:
		case RESPONSE_PREPARE:
		case PREPARED_PARAMETERS:
			offset = mysql_dissect_field_packet(tvb, offset, tree, conn_data);
			break;

		case ROW_PACKET:
			offset = mysql_dissect_row_packet(tvb, offset, tree);
			break;

		case PREPARED_FIELDS:
			offset = mysql_dissect_field_packet(tvb, offset, tree, conn_data);
			break;

		case AUTH_SWITCH_REQUEST:
			offset = mysql_dissect_auth_switch_request(tvb, pinfo, offset, tree, conn_data);
			break;


		default:
			ti = proto_tree_add_item(tree, hf_mysql_payload, tvb, offset, -1, ENC_NA);
			expert_add_info(pinfo, ti, &ei_mysql_unknown_response);
			offset += tvb_reported_length_remaining(tvb, offset);
			mysql_set_conn_state(pinfo, conn_data, UNDEFINED);
		}
	}

	return offset;
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
mysql_dissect_auth_switch_response(struct buffer *buf, packet_info *pinfo, mysql_conn_data_t *conn_data)
{
	int lenstr;
	// col_set_str(pinfo->cinfo, COL_INFO, "Auth Switch Response" );

	/* Data */
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
	buf_retrieveAll(buf);
}