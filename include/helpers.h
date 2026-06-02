#ifndef HELPERS_H
#define HELPERS_H

#include "auth.h"
#include "resources.h"
#include "secrets.h"

typedef struct {
    char* key_type;
    char* key;
} MAYBE_SSH_KEY;


char* load_env_var(const char *str);
const char* build_launch_json(const char *ad, const Credential *creds, const Secrets *secrets, const Resources *res);

#endif