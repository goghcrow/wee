#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "codec.h"
#include "hessian.h"
#include "../base/buffer.h"
#include "../base/cJSON.h"

#define DUBBO_BUF_LEN 8192
#define DUBBO_HDR_LEN 16
#define DUBBO_MAGIC 0xdabb
#define DUBBO_VER "2.0.0"

#define DUBBO_GENERIC_METHOD_NAME "$invokeWithJsonArgs"
#define DUBBO_GENERIC_METHOD_VER "0.0.0"
#define DUBBO_GENERIC_METHOD_PARA_TYPES ""
#define DUBBO_GENERIC_METHOD_ARGC 3
#define DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX 0
#define DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX 1
#define DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX 2

#define DUBBO_HESSIAN2_SERI_ID 2

#define DUBBO_FLAG_REQ 0x80
#define DUBBO_FLAG_TWOWAY 0x40
#define DUBBO_FLAG_EVT 0x20

#define DUBBO_SERI_MASK 0x1f

#define DUBBO_RES_EX 0
#define DUBBO_RES_VAL 1
#define DUBBO_RES_NULL 2

struct dubbo_req
{
    int64_t id;
    bool is_twoway;
    bool is_evt;

    char *service; // java string -> hessian string
    char *method;  // java string -> hessian string
    char **argv;   // java string[] -> hessian string[]
    int argc;
    char *attach; // java map<string, string> -> hessian map<string, string>

    // for evt
    char *data;
    size_t data_sz;
};
struct dubbo_hdr
{
    int8_t flag;
    int8_t status;
    int32_t body_sz;
    int64_t reqid;
};

static int64_t next_reqid()
{
    static int64_t id = 0;
    if (++id == 0x7fffffffffffffff)
    {
        id = 1;
    }
    return id;
}

static char *rebuild_json_args(char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        return NULL;
    }
    if (!cJSON_IsArray(root) && !cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *obj = cJSON_CreateObject();
    int i = 0;
    char buf[10];
    memset(buf, 0, sizeof(buf));

    cJSON *el;
    cJSON_ArrayForEach(el, root)
    {
        sprintf(buf, "arg%d", i++);
        cJSON_AddItemToObject(obj, buf, cJSON_Duplicate(el, true));
    }

    cJSON_Delete(root);
    return cJSON_PrintUnformatted(obj);
}

// fixme
// static char *rebuild_json_attach(char *json_str)
// {
//     return NULL;
// }

static bool encode_req_hdr(struct buffer *buf, struct dubbo_hdr *hdr)
{
    if (buf_prependable(buf) < DUBBO_HDR_LEN)
    {
        return false;
    }

    buf_prependInt32(buf, hdr->body_sz);
    buf_prependInt64(buf, hdr->reqid);
    buf_prependInt8(buf, hdr->status);
    buf_prependInt8(buf, hdr->flag);
    buf_prependInt16(buf, DUBBO_MAGIC); // fixme
    return true;
}

#define write_hs_str(buf, s)                                             \
    {                                                                    \
        int n = hs_encode_string((s), (uint8_t *)buf_beginWrite((buf))); \
        if (n == -1)                                                     \
        {                                                                \
            return false;                                                \
        }                                                                \
        buf_has_written(buf, n);                                         \
    }

static bool encode_req_data(struct buffer *buf, struct dubbo_req *req)
{
    write_hs_str(buf, DUBBO_VER);
    write_hs_str(buf, req->service);
    write_hs_str(buf, DUBBO_GENERIC_METHOD_NAME);
    write_hs_str(buf, DUBBO_GENERIC_METHOD_VER);
    write_hs_str(buf, DUBBO_GENERIC_METHOD_PARA_TYPES);

    // args
    write_hs_str(buf, req->argv[DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX]);
    write_hs_str(buf, req->argv[DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX]);
    write_hs_str(buf, req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX]);

    // fixme :  attach NULL
    buf_has_written(buf, hs_encode_null((uint8_t *)buf_beginWrite(buf)));
    return true;
}

static bool encode_req(struct buffer *buf, struct dubbo_req *req)
{
    struct dubbo_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.reqid = next_reqid();
    hdr.status = 0;
    hdr.flag = DUBBO_FLAG_REQ | DUBBO_HESSIAN2_SERI_ID;
    if (req->is_twoway)
    {
        hdr.flag |= DUBBO_FLAG_TWOWAY;
    }
    if (req->is_evt)
    {
        hdr.flag |= DUBBO_FLAG_EVT;
    }

    if (req->is_evt)
    {
        buf_append(buf, req->data, req->data_sz);
    }
    else
    {
        if (!encode_req_data(buf, req))
        {
            fprintf(stderr, "ERROR: fail to encode req data\n");
            return false;
        }
    }
    hdr.body_sz = buf_readable(buf);

    if (!encode_req_hdr(buf, &hdr))
    {
        fprintf(stderr, "ERROR: fail to encode req hdr\n");
        return false;
    }

    return true;
}

struct dubbo_req *dubbo_req_create(char *service, char *method, char *json_args, char *json_attach)
{
    struct dubbo_req *req = calloc(1, sizeof(*req));
    assert(req);
    req->is_twoway = true;
    req->is_evt = false;
    req->service = strdup(service);
    req->method = strdup(method);

    char *args = rebuild_json_args(json_args);
    if (args == NULL)
    {
        fprintf(stderr, "ERROR invalid json args %s\n", json_args);
        return NULL;
    }

    req->argc = DUBBO_GENERIC_METHOD_ARGC;
    req->argv = calloc(3, sizeof(void *));
    assert(req->argv);
    req->argv[DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX] = strdup(service);
    req->argv[DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX] = strdup(method);
    req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX] = args;

    // fixme 要处理成 hessian map
    req->attach = json_attach;

    return req;
}

void dubbo_req_release(struct dubbo_req *req)
{
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX]);
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX]);
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX]);
    free(req->argv);
    free(req);
}

struct buffer *dubbo_encode(struct dubbo_req *req)
{
    struct buffer *buf = buf_create_ex(DUBBO_BUF_LEN, DUBBO_HDR_LEN);

    if (encode_req(buf, req)) {
        return buf;
    } else {
        buf_release(buf);
        return NULL;
    }
}

bool dubbo_decode(struct buffer *buf, struct dubbo_res *res)
{
    return false;
}

bool is_dubbo_packet(struct buffer *buf)
{
    return false;
}
