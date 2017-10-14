#ifndef DUBBO_CODEC_H
#define DUBBO_CODEC_H

#include <stdbool.h>
#include "../base/buffer.h"

struct dubbo_req;
struct dubbo_res
{
};

struct dubbo_req *dubbo_req_create();
void dubbo_req_release(struct dubbo_req *);

bool dubbo_encode_generic(struct buffer *, struct dubbo_req *);
bool dubbo_decode_generic(struct buffer *, struct dubbo_res *);

bool is_dubbo_packet(struct buffer *);

#endif