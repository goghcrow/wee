#include <stdio.h>
#include "buffer.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void test0()
{
    struct buffer *buf = buf_create(2);
    buf_append(buf, "HELLO_WORLD", strlen("HELLO_WORLD"));
    printf("readabled %zu\n", buf_readable(buf));
    printf("%.*s\n", (int)buf_readable(buf), buf_peek(buf));
    buf_retrieveAll(buf);
    printf("readabled %zu\n", buf_readable(buf));
    buf_release(buf);
}

void test1()
{
    struct buffer *buf = buf_create(2);
    buf_append(buf, "HELLO", 5);
    assert(buf_readable(buf) == 5);
    buf_release(buf);
}

void test2()
{
    struct buffer *buf = buf_create(2);
    assert(buf_writable(buf) == 2);
    buf_appendInt8(buf, 42);
    assert(buf_writable(buf) == 1);
    buf_release(buf);
}

void test3()
{
    struct buffer *buf = buf_create(2);
    assert(buf_prependable(buf) == BufCheapPrepend);
    buf_prependInt32(buf, 42);
    assert(buf_prependable(buf) == BufCheapPrepend - 4);
    buf_release(buf);
}

void test4()
{
    struct buffer *buf = buf_create(2);
    const char *p1 = buf_peek(buf);
    buf_prependInt8(buf, 42);
    const char *p2 = buf_peek(buf);
    assert((p1 - p2) == 1);
    assert(buf_readInt8(buf) == 42);
    buf_release(buf);
}

void test5()
{
    struct buffer *buf = buf_create(100);
    const char *p1 = buf_beginWrite(buf);
    buf_appendInt32(buf, 42);
    const char *p2 = buf_beginWrite(buf);
    assert((p2 - p1) == 4);
    buf_release(buf);
}

void test6()
{
    struct buffer *buf = buf_create(10);
    assert(buf_writable(buf) == 10);
    assert(buf_readable(buf) == 0);
    assert(buf_prependable(buf) == BufCheapPrepend);
    buf_has_written(buf, 5);
    assert(buf_writable(buf) == 5);
    assert(buf_readable(buf) == 5);
    buf_unwrite(buf, 2);
    assert(buf_writable(buf) == 7);
    assert(buf_readable(buf) == 3);
    buf_release(buf);
}

void test7()
{
    struct buffer *buf = buf_create(10);

    buf_append(buf, "HELLO\n!", strlen("HELLO\n!"));
    const char *eol = buf_findEOL(buf);
    assert(*(eol + 1) == '!');
    buf_retrieveAll(buf);
    const char *eol1 = buf_findEOL(buf);
    assert(eol1 == NULL);
    buf_retrieveAll(buf);

    buf_append(buf, "HELLO\r\n!", strlen("HELLO\r\n!"));
    const char *crlf = buf_findCRLF(buf);
    assert(*crlf == '\r');
    assert(*(crlf + 1) == '\n');
    assert(*(crlf + 2) == '!');
    buf_retrieveAll(buf);
    const char *crlf1 = buf_findCRLF(buf);
    assert(crlf1 == NULL);

    buf_release(buf);
}

void test8()
{
    struct buffer *buf = buf_create(2);
    assert(buf_internalCapacity(buf) == BufCheapPrepend + 2);
    buf_appendInt32(buf, 42);
    assert(buf_writable(buf) == 0);
    assert(buf_internalCapacity(buf) == BufCheapPrepend + 2 + 2);
    buf_retrieve(buf, 4);
    assert(buf_readable(buf) == 0);
    assert(buf_writable(buf) == 4);
    assert(buf_internalCapacity(buf) == BufCheapPrepend + 2 + 2);
    buf_shrink(buf, 2);
    assert(buf_internalCapacity(buf) == BufCheapPrepend + 2);
    buf_release(buf);
}

void test9()
{
    struct buffer *buf = buf_create(1);
    buf_appendInt8(buf, 1);
    buf_appendInt16(buf, 2);
    buf_appendInt32(buf, 3);
    buf_appendInt64(buf, 4);
    assert(buf_readInt8(buf) == 1);
    assert(buf_readInt16(buf) == 2);
    assert(buf_readInt32(buf) == 3);
    assert(buf_readInt64(buf) == 4);
    buf_release(buf);
}

void test10()
{
    struct buffer *buf = buf_create(10);
#include <fcntl.h>
    int fd = open("/dev/zero", O_RDONLY);
    if (fd == -1)
    {
        assert(0);
    }
    int err = 0;
    int n = buf_readFd(buf, fd, &err);
    if (n < 0)
    {
        assert(0);
    }

    assert(buf_readable(buf) == 65535 + 10);
    // #include <unistd.h>
    // close(fd);

    buf_release(buf);
}

void test11()
{
    char *hello = "HELLO WORLD\n";
    struct buffer *buf = buf_create(10);
    buf_append(buf, hello, strlen(hello));

    char *str = malloc(strlen(hello) + 1);
    buf_retrieveAsString(buf, strlen(hello), str);
    puts(str);
    free(str);

    buf_release(buf);
}

// save set write idx 用法
void test12()
{
    struct buffer *buf = buf_create(10);
    size_t save_idx = buf_getWriteIndex(buf);
    assert(save_idx == buf_prependable(buf));
    // 暂时跳过2字节
    buf_setWriteIndex(buf, save_idx + 2);

    char *hello = "HELLO";
    char *x = "::";

    buf_append(buf, hello, strlen(hello));
    size_t save_idx2 = buf_getWriteIndex(buf);
    buf_setWriteIndex(buf, save_idx);
    buf_append(buf, x, 2);
    buf_setWriteIndex(buf, save_idx2);

    printf("%7s\n", buf_peek(buf));
    buf_release(buf);
}

void test13()
{
    struct buffer *buf = buf_create(10);
    buf_append(buf, "hello\0", 6);
    const char *c = buf_findChar(buf, '\0');
    assert(c - buf_peek(buf) == 5);
    assert(buf_findChar(buf, 'a') == NULL);
    assert((buf_findStr(buf, "lo") - buf_peek(buf)) == 3);
    assert(buf_findStr(buf, "xx") == NULL);
    buf_release(buf);
}

void test14()
{
    struct buffer *buf = buf_create(10);
    buf_appendInt16LE(buf, 1024);
    assert(buf_readInt16LE(buf) == 1024);
    buf_appendInt32LE(buf, 1024);
    assert(buf_readInt32LE(buf) == 1024);
    buf_appendInt64LE(buf, 1024);
    assert(buf_readInt64LE(buf) == 1024);

    buf_prependInt16LE(buf, 1024);
    assert(buf_readInt16LE(buf) == 1024);
    buf_prependInt32LE(buf, 1024);
    assert(buf_readInt32LE(buf) == 1024);
    buf_prependInt64LE(buf, 1024);
    assert(buf_readInt64LE(buf) == 1024);
    buf_release(buf);
}

void test15()
{
    struct buffer *buf = buf_create(10);

    buf_append(buf, "HELLO\0", 6);
    char *str = buf_readCStr(buf);
    assert(str);
    assert(strcmp("HELLO\0", str) == 0);
    assert(buf_readable(buf) == 0);

    buf_append(buf, "HELLO", 5);
    str = buf_readCStr(buf);
    assert(buf_readable(buf) == 0);

    buf_release(buf);
}

int main(void)
{
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    test8();
    test9();
    test10();
    test11();
    test12();
    test13();
    test14();
    test15();
    return 0;
}
