#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#if defined NDEBUG
#define TRACE(format, ...)
#else
#define TRACE(format, ...) printf("%s::%s(%d)" format, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#endif

#endif