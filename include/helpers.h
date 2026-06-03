#ifndef HELPERS_H
#define HELPERS_H

#include <time.h>

#include "auth.h"
#include "resources.h"
#include "secrets.h"

typedef struct {
    char* key_type;
    char* key;
} MAYBE_SSH_KEY;

typedef struct {
    struct timespec poll_interval;
    struct timespec ad_interval;
} Intervals;

char* load_env_var(const char *str);
char* build_launch_json(const char *ad, const Credential *creds, const Secrets *secrets, const Resources *res);
Intervals setup_intervals(const long poll_secs, const double ad_secs);

#endif