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

#include "../base/dbg.h"

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

#define DUBBO_RES_T_OK 20
#define DUBBO_RES_T_CLIENT_TIMEOUT 30
#define DUBBO_RES_T_SERVER_TIMEOUT 31
#define DUBBO_RES_T_BAD_REQUEST 40
#define DUBBO_RES_T_BAD_RESPONSE 50
#define DUBBO_RES_T_SERVICE_NOT_FOUND 60
#define DUBBO_RES_T_SERVICE_ERROR 70
#define DUBBO_RES_T_SERVER_ERROR 80
#define DUBBO_RES_T_CLIENT_ERROR 90

static char *get_res_status_desc(int8_t status)
{
    switch (status)
    {
    case DUBBO_RES_T_OK:
        return "OK";
    case DUBBO_RES_T_CLIENT_TIMEOUT:
        return "CLIENT TIMEOUT";
    case DUBBO_RES_T_SERVER_TIMEOUT:
        return "SERVER TIMEOUT";
    case DUBBO_RES_T_BAD_REQUEST:
        return "BAD REQUEST";
    case DUBBO_RES_T_BAD_RESPONSE:
        return "BAD RESPONSE";
    case DUBBO_RES_T_SERVICE_NOT_FOUND:
        return "SERVICE NOT FOUND";
    case DUBBO_RES_T_SERVICE_ERROR:
        return "SERVICE ERROR";
    case DUBBO_RES_T_SERVER_ERROR:
        return "SERVER ERROR";
    case DUBBO_RES_T_CLIENT_ERROR:
        return "CLIENT ERROR";
    default:
        return "UNKNOWN";
    }
}

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

static char *rebuild_json_args(const char *json_str)
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

static bool encode_req_hdr(struct buffer *buf, const struct dubbo_hdr *hdr)
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

static bool decode_res_hdr(struct buffer *buf, struct dubbo_hdr *hdr)
{
    if (!is_dubbo_packet(buf))
    {
        return false;
    }

    buf_readInt16(buf); // magic
    hdr->flag = buf_readInt8(buf);
    hdr->status = buf_readInt8(buf);
    hdr->reqid = buf_readInt64(buf);
    hdr->body_sz = buf_readInt32(buf);

    int8_t seria_id = hdr->flag & DUBBO_SERI_MASK;
    if (seria_id != DUBBO_HESSIAN2_SERI_ID)
    {
        fprintf(stderr, "ERROR: unsupport dubbo serialization id %d\n", seria_id);
        return false;
    }

    if ((hdr->flag & DUBBO_FLAG_REQ) == 0)
    {
        // decode response
        return true;
    }
    else
    {
        // decode request
        fprintf(stderr, "ERROR: unsupport decode dubbo request packet\n");
        return false;
    }
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

static bool encode_req_data(struct buffer *buf, const struct dubbo_req *req)
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

#define read_hs_str(buf, out, out_sz)                                                        \
    if (!hs_decode_string((uint8_t *)buf_peek((buf)), buf_readable((buf)), (out), (out_sz))) \
    {                                                                                        \
        fprintf(stderr, "ERROR: failed to decode hessian string\n");                         \
        return false;                                                                        \
    }                                                                                        \
    buf_retrieve(buf, *(out_sz));

static bool decode_res_data(struct buffer *buf, const struct dubbo_hdr *hdr, struct dubbo_res *res)
{
    int32_t flag = -1;
    // decode data
    if (hs_decode_int((uint8_t *)buf_peek(buf), buf_readable(buf), &flag))
    {
        fprintf(stderr, "ERROR: failed to decode hessian int\n");
        return false;
    }

    switch (flag)
    {
    case DUBBO_RES_NULL:
        break;
    case DUBBO_RES_EX:
        read_hs_str(buf, &res->data, &res->data_sz);
        break;
    case DUBBO_RES_VAL:
        read_hs_str(buf, &res->data, &res->data_sz);
        break;
    default:
        fprintf(stderr, "ERROR: Unknown result flag, expect '0' '1' '2', get %d\n", res->type);
        return false;
    }

    res->type = flag;
    return true;
}

void dubbo_res_release(struct dubbo_res *res)
{
    if (res->desc)
    {
        free(res->desc);
    }
    if (res->data)
    {
        free(res->data);
    }
    if (res->attach)
    {
        free(res->attach);
    }
    free(res);
}

static bool encode_req(struct buffer *buf, const struct dubbo_req *req)
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
            fprintf(stderr, "ERROR: failed to encode req data\n");
            return false;
        }
    }
    hdr.body_sz = buf_readable(buf);

    if (!encode_req_hdr(buf, &hdr))
    {
        fprintf(stderr, "ERROR: failed to encode req hdr\n");
        return false;
    }

    return true;
}

static bool decode_res(struct buffer *buf, const struct dubbo_hdr *hdr, struct dubbo_res *res)
{
    res->is_evt = hdr->flag & DUBBO_FLAG_EVT;
    res->desc = strdup(get_res_status_desc(hdr->status));

    if (hdr->status == DUBBO_RES_T_OK)
    {
        res->ok = true;
        if (res->is_evt)
        {
            // decode evt
        }
        else
        {
            if (!decode_res_data(buf, hdr, res))
            {
                fprintf(stderr, "ERROR: failed to decode response data\n");
                return false;
            }
        }
    }
    else
    {
        res->ok = false;
        read_hs_str(buf, &res->data, &res->data_sz);
    }

    // fixme read attach

    return true;
}

struct dubbo_req *dubbo_req_create(const char *service, const char *method, const char *json_args, const char *json_attach)
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
    if (json_attach)
    {
        req->attach = strdup(json_attach);
    }

    return req;
}

void dubbo_req_release(struct dubbo_req *req)
{
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX]);
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX]);
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX]);
    free(req->argv);
    if (req->attach)
    {
        free(req->attach);
    }
    free(req);
}

struct buffer *dubbo_encode(const struct dubbo_req *req)
{
    struct buffer *buf = buf_create_ex(DUBBO_BUF_LEN, DUBBO_HDR_LEN);

    if (encode_req(buf, req))
    {
        return buf;
    }
    else
    {
        buf_release(buf);
        return NULL;
    }
}

struct dubbo_res *dubbo_decode(struct buffer *buf)
{
    struct dubbo_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    if (!decode_res_hdr(buf, &hdr))
    {
        fprintf(stderr, "ERROR: failed to decode dubbo response");
        return NULL;
    }

    struct dubbo_res *res = calloc(1, sizeof(*res));
    assert(res);
    res->type = -1;

    if (!(decode_res(buf, &hdr, res)))
    {
        free(res);
        return NULL;
    }

    return res;
}

bool is_dubbo_packet(const struct buffer *buf)
{
    return buf_readable(buf) >= DUBBO_HDR_LEN && (uint16_t)buf_peekInt16(buf) == DUBBO_MAGIC;
}