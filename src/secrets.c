#include <stdlib.h>
#include <string.h>

#include "secrets.h"
#include "helpers.h"
#include "log.h"

int new_secrets(Secrets *secrets, const char *image_id, const char *subnet_id, const char *ssh_key, ExtraMap *extra) {
    if (secrets == NULL) {
        LOG_ERROR("Failed to create new secrets, passed a NULL reference of Secrets!");
        return -1;
    }

    if (image_id == NULL || subnet_id == NULL || ssh_key == NULL) {
        LOG_ERROR("Failed to create new secrets, one of the required fields wasn't provided!");
        return -1;
    }

    secrets->extras = extra;
    secrets->image_id = strdup(image_id);
    secrets->ssh_key = strdup(ssh_key);
    secrets->subnet_id = strdup(subnet_id);

    return 0;
}

int new_secrets_from_env(Secrets *secrets) {
    if (secrets == NULL) {
        LOG_ERROR("Failed to create new secrets, passed a NULL reference of Secrets!");
        return -1;
    }

    char *image_id = load_env_var("IMAGE_OCID");
    char *subnet_id = load_env_var("SUBNET_OCID");
    char *ssh_key = load_env_var("SSH_PUBLIC_KEY");

    if (image_id == NULL || subnet_id == NULL || ssh_key == NULL) {
        free(image_id);
        free(subnet_id);
        free(ssh_key);

        LOG_ERROR("Failed to read environment variables! (IMAGE_OCID, SUBNET_OCID, SSH_PUBLIC_KEY)");
        return -1;
    }

    secrets->extras = NULL;
    secrets->image_id = image_id;
    secrets->subnet_id = subnet_id;
    secrets->ssh_key = ssh_key;

    return 0;
}