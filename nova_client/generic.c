#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "generic.h"
#include "codec.h"
#include "buffer.h"

#define GENERIC_SERVICE "com.youzan.nova.framework.generic.service.GenericService"
#define GENERIC_SERVICE_LEN 56
#define GENERIC_METHOD "invoke"
#define GENERIC_METHOD_LEN 6
#define GENERIC_COMMON_LEN 44

#define VER_MASK 0xffff0000
#define VER1 0x80010000

#define T_CALL 1
#define T_REPLY 2
#define T_EX 3

#define TYPE_STRUCT 12
#define TYPE_STRING 11

#define FIELD_STOP 0

struct buffer *thrift_generic_pack(int seq,
                                   const char *serv, int serv_len,
                                   const char *method, int method_len,
                                   const char *json_args, int args_len)
{
    struct buffer *buf = buf_create(GENERIC_COMMON_LEN + serv_len + method_len + args_len);
    buf_appendInt32(buf, VER1 | T_CALL);
    buf_appendInt32(buf, GENERIC_METHOD_LEN);
    buf_append(buf, GENERIC_METHOD, GENERIC_METHOD_LEN);
    buf_appendInt32(buf, seq);

    // pack args
    {
        uint16_t field_id = 1;
        uint8_t field_type = TYPE_STRUCT;
        buf_appendInt8(buf, field_type);
        buf_appendInt16(buf, field_id);
    }

    // pack struct \Com\Youzan\Nova\Framework\Generic\Service\GenericRequest
    {
        {
            uint16_t field_id = 1;
            uint8_t field_type = TYPE_STRING;
            buf_appendInt8(buf, field_type);
            buf_appendInt16(buf, field_id);
            buf_appendInt32(buf, serv_len);
            buf_append(buf, serv, serv_len);
        }
        {
            uint16_t field_id = 2;
            uint8_t field_type = TYPE_STRING;
            buf_appendInt8(buf, field_type);
            buf_appendInt16(buf, field_id);
            buf_appendInt32(buf, method_len);
            buf_append(buf, method, method_len);
        }
        {
            uint16_t field_id = 3;
            uint8_t field_type = TYPE_STRING;
            buf_appendInt8(buf, field_type);
            buf_appendInt16(buf, field_id);
            buf_appendInt32(buf, args_len);
            buf_append(buf, json_args, args_len);
        }
        buf_appendInt8(buf, FIELD_STOP);
    }

    buf_appendInt8(buf, FIELD_STOP);

    return buf;
}

bool thrift_generic_unpack(struct buffer *buf, char **out)
{
    uint32_t ver1 = (uint32_t)buf_readInt32(buf);
    if (ver1 > 0x7fffffff)
    {
        ver1 = 0 - ((ver1 - 1) ^ 0xffffffff);
    }
    ver1 = ver1 & VER_MASK;
    if (ver1 != VER1)
    {
        fprintf(stderr, "unexpected thrift protocol version\n");
        return false;
    }

    int type = ver1 & 0x000000ff;
    if (type == T_EX)
    {
        fprintf(stderr, "unexpected thrift exception response\n");
        return false;
    }

    uint32_t method_len = (uint32_t)buf_readInt32(buf);
    char *method = malloc(method_len + 1);
    buf_retrieveAsString(buf, method_len, method);
    if (strncmp(method, GENERIC_METHOD, GENERIC_COMMON_LEN) != 0)
    {
        fprintf(stderr, "unexpected generic method name:%.*s\n", GENERIC_COMMON_LEN, method);
        return false;
    }
    free(method);

    uint32_t seq = (uint32_t)buf_readInt32(buf);
    (void)seq;

    uint8_t field_type = buf_readInt8(buf);
    uint16_t field_id = (uint16_t)buf_readInt16(buf);
    assert(field_id == 0);
    if (field_type != TYPE_STRING)
    {
        fprintf(stderr, "unexpected generic idl format: expected TYPE_STRING but got %c\n", field_type);
        return false;
    }
    uint32_t resp_len = (uint32_t)buf_readInt32(buf);
    char *resp = malloc(resp_len + 1);
    buf_retrieveAsString(buf, resp_len, resp);
    *out = resp;

    field_type = buf_readInt8(buf);
    assert(field_type == FIELD_STOP);
    return true;
}

struct nova_hdr *create_nova_generic(const char *attach)
{
    struct nova_hdr *hdr = nova_hdr_create();

    hdr->magic = NOVA_MAGIC;
    hdr->version = 1;
    hdr->ip = 0;
    hdr->port = 0;
    hdr->service_len = GENERIC_SERVICE_LEN;
    hdr->method_len = GENERIC_METHOD_LEN;
    hdr->attach_len = strlen(attach);

    int head_sz = NOVA_HEADER_COMMON_LEN + hdr->service_len + hdr->method_len + hdr->attach_len;
    if (head_sz > 0x7fff)
    {
        fprintf(stderr, "ERROR, too large nova header as %d\n", head_sz);
        return NULL;
    }

    hdr->head_size = (int16_t)head_sz;
    hdr->service_name = malloc(hdr->service_len + 1);
    assert(hdr->service_name != NULL);
    memcpy(hdr->service_name, GENERIC_SERVICE, hdr->service_len);
    hdr->service_name[hdr->service_len] = 0;

    hdr->method_name = malloc(hdr->method_len + 1);
    assert(hdr->method_name != NULL);
    memcpy(hdr->method_name, GENERIC_METHOD, hdr->method_len);
    hdr->method_name[hdr->method_len] = 0;
    hdr->seq_no = 1;

    hdr->attach = malloc(hdr->attach_len + 1);
    assert(hdr->attach != NULL);
    memcpy(hdr->attach, attach, hdr->attach_len);
    hdr->attach[hdr->attach_len] = 0;
    return hdr;
}
