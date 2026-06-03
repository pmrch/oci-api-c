#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include "auth.h"
#include "resources.h"
#include "secrets.h"

typedef struct {
    Credential *credentials;
    Secrets *secrets;
    ResourceConfig *res;
} AppContext;

void free_app_context(AppContext *ctx);
AppContext *setup_app_context(const char *config_path, const char *oci_config_path);

#endif