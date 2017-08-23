#ifndef NOVA_H
#define NOVA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NOVA_MAGIC 0xdabc
#define NOVA_HEADER_COMMON_LEN 37

struct nova_hdr
{
    int32_t msg_size;
    uint16_t magic;
    int16_t head_size;
    int8_t version;
    uint32_t ip;
    uint32_t port;
    int32_t service_len;
    char *service_name;
    int32_t method_len;
    char *method_name;
    int64_t seq_no;
    int32_t attach_len;
    char *attach;
};

struct nova_hdr *nova_hdr_create();

void nova_hdr_release(struct nova_hdr *hdr);

bool nova_detect(const char* buf, size_t size);

void nova_pack(struct buffer *, struct nova_hdr *hdr, const char *body, size_t body_size);

bool nova_unpack(struct buffer *, struct nova_hdr *hdr);

#endif