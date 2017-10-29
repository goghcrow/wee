#ifndef _THRIFT_GENERIC_H_
#define _THRIFT_GENERIC_H_

#include <stdbool.h>

struct buffer *thrift_generic_pack(int seq,
                                   const char *serv, int serv_len,
                                   const char *method, int method_len,
                                   const char *json_args, int args_len);

bool thrift_generic_unpack(struct buffer *buf, char **out);

struct nova_hdr *create_nova_generic(const char *attach);

#endif