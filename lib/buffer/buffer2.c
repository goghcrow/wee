
#define CheapPrepend 8
#define InitialSize 1024

#include <unistd.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
// mac找不到endian.h   ln -s /usr/include/machine/endian.h /usr/local/include/endian.h
#include <endian.h>
#include <errno.h>
#include "buffer.h"


// TODO
// 测试用例同一套，同一个header
// 做一个新的 buffer r_idx w_idx 换成char * 或者void *实现
// 重新申请内存 后，更新 读写指针

// !!!!!


struct buffer
{
    size_t r_idx;
    size_t w_idx;
    size_t sz;
    void *buf;
};

struct buffer *buf_create(size_t size)
{
    if (size == 0)
    {
        size = 1024;
    }
    size_t sz = size + CheapPrepend;
    struct buffer *buf = malloc(sizeof(*buf));
    assert(buf);
    memset(buf, 0, sizeof(*buf));
    buf->buf = malloc(sz);
    assert(buf->buf);
    memset(buf->buf, 0, sz);
    buf->sz = sz;
    buf->r_idx = CheapPrepend;
    buf->w_idx = CheapPrepend;
    return buf;
}

void buf_release(struct buffer *buf)
{
    free(buf->buf);
    free(buf);
}

size_t buf_readable(struct buffer *buf)
{
    return buf->w_idx - buf->r_idx;
}

size_t buf_writable(struct buffer *buf)
{
    return buf->sz - buf->w_idx;
}

size_t buf_prependable(struct buffer *buf)
{
    return buf->r_idx;
}

const char *buf_peek(struct buffer *buf)
{
    return buf->buf + buf->r_idx;
}

char *buf_beginWrite(struct buffer *buf)
{
    return buf->buf + buf->w_idx;
}

void buf_has_written(struct buffer *buf, size_t len)
{
    assert(len <= buf_writable(buf));
    buf->w_idx += len;
}

void buf_unwrite(struct buffer *buf, size_t len)
{
    assert(len <= buf_readable(buf));
    buf->w_idx -= len;
}

const char *buf_findCRLF(struct buffer *buf)
{
    return (char *)memmem(buf_peek(buf), buf_readable(buf), "\r\n", 2);
}

const char *buf_findEOL(struct buffer *buf)
{
    return (char *)memchr(buf_peek(buf), '\n', buf_readable(buf));
}

void buf_retrieveAll(struct buffer *buf)
{
    buf->r_idx = CheapPrepend;
    buf->w_idx = CheapPrepend;
}

void buf_retrieve(struct buffer *buf, size_t len)
{
    assert(len <= buf_readable(buf));
    if (len < buf_readable(buf))
    {
        buf->r_idx += len;
    }
    else
    {
        buf_retrieveAll(buf);
    }
}

void buf_retrieveUntil(struct buffer *buf, const char *end)
{
    assert(buf_peek(buf) <= end);
    assert(end <= buf_beginWrite(buf));
    buf_retrieve(buf, end - buf_peek(buf));
}

void buf_retrieveInt64(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int64_t));
}

void buf_retrieveInt32(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int32_t));
}

void buf_retrieveInt16(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int16_t));
}

void buf_retrieveInt8(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int8_t));
}

static void buf_swap(struct buffer *buf, size_t nsz)
{
    // FIX nsz > buf->size realloc ?
    assert(nsz >= buf_readable(buf));
    void *nbuf = malloc(nsz);
    assert(nbuf);
    memset(nbuf, 0, nsz);
    memcpy(nbuf + CheapPrepend, buf_peek(buf), buf_readable(buf));
    free(buf->buf);
    buf->buf = nbuf;
    buf->sz = nsz;
}

static void buf_makeSpace(struct buffer *buf, size_t len)
{
    size_t readable = buf_readable(buf);
    if (buf_prependable(buf) + buf_writable(buf) - CheapPrepend < len)
    {
        size_t nsz = buf->w_idx + len;
        buf_swap(buf, nsz);
    }
    else
    {
        assert(CheapPrepend < buf->r_idx);
        memmove(buf->buf + CheapPrepend, buf_peek(buf), readable);
    }

    buf->r_idx = CheapPrepend;
    buf->w_idx = CheapPrepend + readable;
    assert(readable == buf_readable(buf));
}

static void buf_ensureWritable(struct buffer *buf, size_t len)
{
    if (buf_writable(buf) < len)
    {
        buf_makeSpace(buf, len);
    }
    assert(buf_writable(buf) >= len);
}

void buf_append(struct buffer *buf, const char *data, size_t len)
{
    buf_ensureWritable(buf, len);
    memcpy(buf_beginWrite(buf), data, len);
    buf_has_written(buf, len);
}

void buf_prepend(struct buffer *buf, const char *data, size_t len)
{
    assert(len <= buf_prependable(buf));
    buf->r_idx -= len;
    memcpy(buf_peek(buf), data, len);
}

void buf_shrink(struct buffer *buf, size_t reserve)
{
    buf_swap(buf, buf_readable(buf) + reserve);
}

void buf_appendInt64(struct buffer *buf, int64_t x)
{
    int64_t be64 = htobe64(x);
    buf_append(buf, &be64, sizeof(int64_t));
}

void buf_appendInt32(struct buffer *buf, int32_t x)
{
    int32_t be32 = htobe32(x);
    buf_append(buf, &be32, sizeof(int32_t));
}

void buf_appendInt16(struct buffer *buf, int16_t x)
{
    int16_t be16 = htobe16(x);
    buf_append(buf, &be16, sizeof(int16_t));
}

void buf_appendInt8(struct buffer *buf, int8_t x)
{
    buf_append(buf, &x, sizeof(int8_t));
}

void buf_prependInt64(struct buffer *buf, int64_t x)
{
    int64_t be64 = htobe64(x);
    buf_prepend(buf, &be64, sizeof(int64_t));
}

void buf_prependInt32(struct buffer *buf, int32_t x)
{
    int32_t be32 = htobe32(x);
    buf_prepend(buf, &be32, sizeof(int32_t));
}

void buf_prependInt16(struct buffer *buf, int16_t x)
{
    int16_t be16 = htobe16(x);
    buf_prepend(buf, &be16, sizeof(int16_t));
}

void buf_prependInt8(struct buffer *buf, int8_t x)
{
    buf_prepend(buf, &x, sizeof(int8_t));
}

int64_t buf_peekInt64(struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int64_t));
    int64_t be64 = 0;
    memcpy(&be64, buf_peek(buf), sizeof(int64_t));
    return be64toh(be64);
}

int32_t buf_peekInt32(struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int32_t));
    int32_t be32 = 0;
    memcpy(&be32, buf_peek(buf), sizeof(int32_t));
    return be32toh(be32);
}

int16_t buf_peekInt16(struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int16_t));
    int16_t be16 = 0;
    memcpy(&be16, buf_peek(buf), sizeof(int16_t));
    return be16toh(be16);
}

int8_t buf_peekInt8(struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int8_t));
    return *buf_peek(buf);
}

int64_t buf_readInt64(struct buffer *buf)
{
    int64_t x = buf_peekInt64(buf);
    buf_retrieveInt64(buf);
    return x;
}

int32_t buf_readInt32(struct buffer *buf)
{
    int32_t x = buf_peekInt32(buf);
    buf_retrieveInt32(buf);
    return x;
}

int16_t buf_readInt16(struct buffer *buf)
{
    int16_t x = buf_peekInt16(buf);
    buf_retrieveInt16(buf);
    return x;
}

int8_t buf_readInt8(struct buffer *buf)
{
    int8_t x = buf_peekInt8(buf);
    buf_retrieveInt8(buf);
    return x;
}

size_t buf_internalCapacity(struct buffer *buf)
{
    return buf->sz;
}

ssize_t buf_readFd(struct buffer *buf, int fd, int *errno_)
{
    char extrabuf[65535];
    struct iovec vec[2];
    size_t writable = buf_writable(buf);
    vec[0].iov_base = buf_beginWrite(buf);
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    int iovcnt = writable < sizeof(extrabuf) ? 2 : 1;
    ssize_t n = readv(fd, vec, iovec);
    if (n < 0)
    {
        *errno_ = n;
    }
    else if (n <= writable)
    {
        buf->w_idx += n;
    }
    else
    {
        buf->w_idx = buf->sz;
        buf_append(buf, n - writable);
    }

    return n;
}