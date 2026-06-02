#include "timestamp.h"
#include <time.h>

Timestamp timestamp_now() {
    return (Timestamp){ .seconds = time(NULL) };
}

void timestamp_format_http_date(const Timestamp ts, char *buf, const size_t len) {
    struct tm *t = gmtime(&ts.seconds);
    strftime(buf, len, "%a, %d %b %Y %T GMT", t);
}

void timestamp_format_iso8601(const Timestamp ts, char *buf, const size_t len) {
    struct tm *t = gmtime(&ts.seconds);
    strftime(buf, len, "%Y%m%dT%H%M%SZ", t);
}

void timestamp_format_date(const Timestamp ts, char *buf, const size_t len) {
    struct tm *t = gmtime(&ts.seconds);
    strftime(buf, len, "%Y%m%d", t);
}

