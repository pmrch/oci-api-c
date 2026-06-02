#include <stdio.h>
#include <time.h>

#include "log.h"

static const char *log_level_str[] = { "INFO", "WARN", "ERROR" };
void log_msg(LogLevel level, const char *fmt, ...) {
    char timebuf[32];
    time_t now = time(NULL);
    struct tm *t = gmtime(&now);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", t);

    fprintf(stderr, "%s [%s] ", timebuf, log_level_str[level]);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

#define LOG_INFO(fmt, ...) log_msg(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_msg(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_msg(LOG_ERROR, fmt, ##__VA_ARGS__)