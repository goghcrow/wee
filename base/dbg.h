#ifndef _DEBUGGER_H_
#define _DEBUGGER_H_

#include <unistd.h>
#include <stdarg.h>

void register_jit_debugger();
void debugger();

// ****************************************************************************************************

void die(const char *fmt, ...);
// bin to printable
void bin2p(const char *bin, int sz);
void bin2hex(const char *bin, size_t sz);

#define DUMP_STRUCT(sp) bin2hex((const char *)(sp), sizeof(*(sp)))
#define DUMP_MEM(vp, n) bin2hex((const char *)(vp), (size_t)(n))

#define UNUSED(x) ((void)(x))

#define LOG_INFO(fmt, ...) \
    fprintf(stderr, "\x1B[1;32mERROR: " fmt "\x1B[0m in function %s %s:%d\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "\x1B[1;31mERROR: " fmt "\x1B[0m in function %s %s:%d\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);

#define PANIC(fmt, ...)                                                                                                     \
    fprintf(stderr, "\x1B[1;31mERROR: " fmt "\x1B[0m in function %s %s:%d\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__); \
    exit(1);

#endif