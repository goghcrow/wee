#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

static int log_level = 0;

static const char *level_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static const char *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};

void log_setlevel(int level)
{
    level = level;
}

void log_log(int level, const char *func, const char *file, int line, const char *fmt, ...)
{
    if (level < log_level)
    {
        return;
    }

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    char buf[16];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
    
    fprintf(stderr, "[%s %s%5s]\x1b[0m ", buf, level_colors[level], level_names[level]);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, " \x1b[2;90min func %s %s:%d\x1b[0m\n", func, file, line);
}