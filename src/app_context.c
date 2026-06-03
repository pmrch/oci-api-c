#include <stdlib.h>
#include <unistd.h>

#include "app_context.h"
#include "auth.h"
#include "log.h"
#include "resources.h"

void free_app_context(AppContext *ctx) {
    if (ctx->credentials) { free_credentials(ctx->credentials); }
    if (ctx->secrets) { free_secrets(ctx->secrets); }
    if (ctx->res) { free_resource_config(ctx->res); }
    free(ctx);
}

AppContext *setup_app_context(const char *config_path, const char *oci_config_path) {
    AppContext *ctx = calloc(1, sizeof(AppContext));
    if (ctx == NULL) {
        LOG_ERROR("Failed to allocate AppContext!");
        return NULL;
    }

    Credential *credentials = load_creds_from_config(oci_config_path);
    if (credentials == NULL) { 
        free_app_context(ctx);
        return NULL; 
    }

    ctx->credentials = credentials;

    Secrets *secrets = new_secrets_from_env();
    if (secrets == NULL) {
        free_app_context(ctx);
        return NULL; 
    }

    ctx->secrets = secrets;

    ResourceConfig *res = load_resources_from_config(config_path);
    if (res == NULL) {
        LOG_ERROR("Failed to load resources from config file '%s'!\n", config_path);
        free_app_context(ctx);
        return NULL;
    }

    ctx->res = res;
    return ctx;
}