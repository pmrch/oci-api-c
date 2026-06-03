#ifndef SECRETS_H
#define SECRETS_H

#include <stddef.h>

typedef struct {
    char* key;
    char* value;
    size_t index;
} ExtraMap;

typedef struct {
    char* image_id;
    char* subnet_id;
    char* ssh_key;
    ExtraMap* extras;
} Secrets;

int new_secrets(Secrets *secrets, const char *image_id, const char *subnet_id, const char *ssh_key, ExtraMap *extra);
Secrets* new_secrets_from_env();
void free_secrets(Secrets *secrets);

#endif