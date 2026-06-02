#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include "timestamp.h"

typedef struct {
    char* tenancy;
    char* user;
    char* key_file;
    char* fingerprint;
    char* region;
    Timestamp* expires_in;
} Credential;

int load_creds_from_config(Credential *credentials, const char *str);
bool is_valid_credential(Credential *credentials);

#endif