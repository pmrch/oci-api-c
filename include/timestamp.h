#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stddef.h>
#include <time.h>
#include <string.h>

typedef struct {
    time_t seconds;
} Timestamp;

Timestamp timestamp_now();

void timestamp_format_iso8601(const Timestamp ts, char *buf, const size_t len);
void timestamp_format_date(const Timestamp ts, char *buf, const size_t len);
void timestamp_format_http_date(const Timestamp ts, char *buf, const size_t len);

#endif