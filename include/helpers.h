#ifndef HELPERS_H
#define HELPERS_H

#include <time.h>

#include "app_context.h"

typedef struct {
    char* key_type;
    char* key;
} MAYBE_SSH_KEY;

typedef struct {
    struct timespec poll_interval;
    struct timespec ad_interval;
} Intervals;

typedef struct {
  char *memory;
  size_t size;
} MemoryStruct;

void to_lowercase(char *str);

char* load_env_var(const char *str);
char* build_launch_json(const AppContext *ctx, const char *ad);

Intervals setup_intervals(const long poll_secs, const double ad_secs);
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

#endif