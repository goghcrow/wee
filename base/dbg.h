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


#endif