#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "codec.h"
#include "../base/buffer.h"
#include "../base/cJSON.h"

#define DUBBO_HDR_LEN 16
#define DUBBO_MAGIC 0xdabb

#define DUBBO_FLAG_REQUEST 0x80
#define DUBBO_FLAG_TWOWAY 0x40
#define DUBBO_FLAG_EVENT 0x20

#define DUBBO_SERIALIZATION_MASK 0x1f

#define DUBBO_RES_EXCEPTION 0
#define DUBBO_RES_VALUE 1
#define DUBBO_RES_NULL 2

struct dubbo_req
{
};

struct dubbo_hdr
{
};

static int64_t next_reqid()
{
    static int64_t id = 0;
    if (int64_t == 0x7fffffffffffffff)
    {
        id = 0;
    }
    return ++id;
}

struct dubbo_req *dubbo_req_create()
{
}

void dubbo_req_release(struct dubbo_req *req)
{
}

bool dubbo_encode_generic(struct buffer *buf, struct dubbo_req *req)
{
}

bool dubbo_decode_generic(struct buffer *buf, struct dubbo_res *res)
{
}

bool is_dubbo_packet(struct buffer *buf)
{
}
