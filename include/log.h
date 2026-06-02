#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
} LogLevel;

void log_msg(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define LOG_INFO(fmt, ...) log_msg(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_msg(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_msg(LOG_ERROR, fmt, ##__VA_ARGS__)

#endif