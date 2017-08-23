#define _GNU_SOURCE
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "../base/endian.h"
#include "../base/buffer.h"
#include "novacodec.h"

struct nova_hdr;

struct nova_hdr *nova_hdr_create()
{
    struct nova_hdr *hdr = malloc(sizeof(*hdr));
    assert(hdr != NULL);
    memset(hdr, 0, sizeof(*hdr));
    return hdr;
}

void nova_hdr_release(struct nova_hdr *hdr)
{
    assert(hdr != NULL);
    free(hdr->method_name);
    free(hdr->service_name);
    free(hdr->attach);
    free(hdr);
}

bool nova_detect(struct buffer *buf)
{
    if (buf_readable(buf) < NOVA_HEADER_COMMON_LEN)
    {
        return false;
    }

    int16_t magic = 0;
    memcpy(&magic, buf_peek(buf) + 4, sizeof(int16_t));
    return be16toh(magic) == NOVA_MAGIC;
}

void nova_pack(struct buffer *buf, struct nova_hdr *hdr, const char *body, size_t body_size)
{
    buf_appendInt32(buf, hdr->head_size + body_size);
    buf_appendInt16(buf, hdr->magic);
    buf_appendInt16(buf, hdr->head_size);
    buf_appendInt8(buf, hdr->version);
    buf_appendInt32(buf, hdr->ip);
    buf_appendInt32(buf, hdr->port);
    buf_appendInt32(buf, hdr->service_len);
    buf_append(buf, hdr->service_name, hdr->service_len);
    buf_appendInt32(buf, hdr->method_len);
    buf_append(buf, hdr->method_name, hdr->method_len);
    buf_appendInt64(buf, hdr->seq_no);
    buf_appendInt32(buf, hdr->attach_len);
    buf_append(buf, hdr->attach, hdr->attach_len);
    buf_append(buf, body, body_size);
}

bool nova_unpack(struct buffer *buf, struct nova_hdr *hdr)
{
    if (buf_readable(buf) <= NOVA_HEADER_COMMON_LEN)
    {
        return false;
    }

    hdr->msg_size = buf_readInt32(buf);
    if (hdr->msg_size <= NOVA_HEADER_COMMON_LEN)
    {
        return false;
    }

    if ((uint16_t)buf_readInt16(buf) != NOVA_MAGIC)
    {
        return false;
    };

    hdr->head_size = buf_readInt16(buf);
    if (hdr->head_size > hdr->msg_size)
    {
        return false;
    }

    hdr->version = buf_readInt8(buf);

    hdr->ip = (uint32_t)buf_readInt32(buf);
    hdr->port = (uint32_t)buf_readInt32(buf);

    hdr->service_len = buf_readInt32(buf);
    hdr->service_name = malloc(hdr->service_len + 1);
    buf_retrieveAsString(buf, hdr->service_len, hdr->service_name);

    hdr->method_len = buf_readInt32(buf);
    hdr->method_name = malloc(hdr->method_len + 1);
    buf_retrieveAsString(buf, hdr->method_len, hdr->method_name);

    hdr->seq_no = buf_readInt64(buf);

    hdr->attach_len = buf_readInt32(buf);
    hdr->attach = malloc(hdr->attach_len + 1);
    buf_retrieveAsString(buf, hdr->attach_len, hdr->attach);

    return true;
}