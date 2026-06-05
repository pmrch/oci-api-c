#include <stddef.h>
#include <stdlib.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h> 
#include <stdlib.h>
#include <string.h>
#include <ini.h>
#include <time.h>

#include "auth.h"
#include "log.h"
#include "timestamp.h"

#define GET_HOME_PATH() (getenv("USERPROFILE") ? getenv("USERPROFILE") : getenv("HOME"))

// Add this macro or a helper function at the top of src/auth.c
#define SAFE_REPLACE(ptr, val) do { \
    if (ptr) { \
        free(ptr); \
        (ptr) = NULL; \
    } \
    (ptr) = strdup(val); \
} while (0)

void free_credentials(Credential *credentials) {
    free(credentials->fingerprint);
    free(credentials->expires_in);
    free(credentials->key_file);
    free(credentials->tenancy);
    free(credentials->region);
    free(credentials->user);
    free(credentials);
}

int verify_path(const char *given, char *dest, const size_t dest_size) {
    // If there was a given path, copy it to final path
    if (given != NULL) {
        size_t src_len = strlen(given);
        memcpy(dest, given, src_len);

        if (src_len < dest_size) { dest[src_len] = '\0'; }
        else { dest[dest_size - 1] = '\0'; }

        return 0;
    }

    // Get home path if there was no given path
    char *home_path = GET_HOME_PATH();
    if (home_path == NULL) {
        home_path = getpwuid(getuid())->pw_dir;
    }

    // Return early if home path was not found
    if (home_path == NULL) {
        LOG_ERROR("Failed to get home path and no path was provided!");
        return -1;
    }

    // If there is enough capacity for the new path, copy it
    size_t needed = strlen(home_path) + 6; // 5 for "/.oci", 1 for '\0'
    if (dest_size < needed) {
        LOG_ERROR("Destination buffer too small for <%s/.oci> (needs %zu, got %zu)", home_path, needed, dest_size);
        return -2;
    }

    sprintf(dest, "%s/.oci", home_path);
    return 0;
}

static int handler(void *creds, const char *section, const char *name, const char *value) {
    Credential *credentials = (Credential*)creds;

    #define MATCH_FIELD(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH_FIELD("DEFAULT", "user"))             SAFE_REPLACE(credentials->user, value);
    else if (MATCH_FIELD("DEFAULT", "fingerprint")) SAFE_REPLACE(credentials->fingerprint, value);
    else if (MATCH_FIELD("DEFAULT", "tenancy"))     SAFE_REPLACE(credentials->tenancy, value);
    else if (MATCH_FIELD("DEFAULT", "region"))      SAFE_REPLACE(credentials->region, value);
    else if (MATCH_FIELD("DEFAULT", "key_file")) {
        // Handle your logic for key_file, ensuring you call free() on the old one first
        if (credentials->key_file) { free(credentials->key_file); }
        
        if (value[0] == '~') {
            const char *home = GET_HOME_PATH();
            char *path = malloc(strlen(home) + strlen(value)); // simplified size
            sprintf(path, "%s%s", home, value + 1);
            credentials->key_file = path;
        } else {
            credentials->key_file = strdup(value);
        }
    } else {
        return 0;
    }
    return 1;
}

bool credentials_any_null(const Credential *credentials) {
    return  credentials->fingerprint == NULL || credentials->key_file == NULL
        || credentials->region == NULL || credentials->tenancy == NULL;
}

Credential* load_creds_from_config(const char *str) {    
    Credential *credentials = calloc(1, sizeof(Credential));
    if (credentials == NULL) {
        LOG_ERROR("Credentials failed to be allocated!");
        return NULL;
    }

    char sure_path[50];
    int verified = verify_path(str, sure_path, 50);
    if (verified != 0) {
        LOG_ERROR("Failed to verify config path!");
        free_credentials(credentials);
        free(credentials);
        return NULL;
    }

    size_t final_size = sizeof(sure_path) + sizeof("/config");
    char filename[final_size];

    snprintf(filename, final_size, "%s/config", sure_path);
    int ini_parse_result = ini_parse(filename, handler, credentials);
    if (ini_parse_result < 0) {
        LOG_ERROR("Can't load %s\n", filename);
        free_credentials(credentials);
        free(credentials);
        return NULL;
    }

    return credentials;
}

bool is_valid_credential(Credential *credentials) {
    bool cond = credentials == NULL || credentials->user == NULL || credentials->fingerprint == NULL
        || credentials->key_file == NULL || credentials->region == NULL || credentials->tenancy == NULL;
    
    if (cond) {
        return false;
    }

    // Take 120s as buffer to avoid edge cases.
    if (credentials->expires_in != NULL) {
        Timestamp now = timestamp_now();
        return credentials->expires_in->seconds > (now.seconds + 120);
    }

    return true;
}