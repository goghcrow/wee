#ifndef LOG_H
#define LOG_H

#include <stdio.h>

enum
{
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

void log_setlevel(int level);
void log_log(int level, const char *func, const char *file, int line, const char *fmt, ...);

#define log_trace(...) log_log(LOG_TRACE, __func__, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __func__, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __func__, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_log(LOG_WARN, __func__, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __func__, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __func__, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_TRACE(fmt, ...) fprintf(stderr, "\x1B[1;94m[TRACE]\x1B[0m " fmt " \x1b[2;90min func %s %s:%d\x1b[0m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);
#define LOG_DEBUG(fmt, ...) fprintf(stderr, "\x1B[1;36m[DEBUG]\x1B[0m " fmt " \x1b[2;90min func %s %s:%d\x1b[0m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);
#define LOG_INFO(fmt, ...)  fprintf(stderr, "\x1B[1;32m[ INFO]\x1B[0m " fmt " \x1b[2;90min func %s %s:%d\x1b[0m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);
#define LOG_WARN(fmt, ...)  fprintf(stderr, "\x1B[1;33m[ WARN]\x1B[0m " fmt " \x1b[2;90min func %s %s:%d\x1b[0m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);
#define LOG_ERROR(fmt, ...) fprintf(stderr, "\x1B[1;31m[ERROR]\x1B[0m " fmt " \x1b[2;90min func %s %s:%d\x1b[0m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);
#define LOG_FATAL(fmt, ...) fprintf(stderr, "\x1B[1;35m[FATAL]\x1B[0m " fmt " \x1b[2;90min func %s %s:%d\x1b[0m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);

#endif