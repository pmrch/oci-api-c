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
    if (MATCH_FIELD("DEFAULT", "user")) { credentials->user = strdup(value); }
    else if (MATCH_FIELD("DEFAULT", "fingerprint")) { credentials->fingerprint = strdup(value); }
    else if (MATCH_FIELD("DEFAULT", "tenancy")) { credentials->tenancy = strdup(value); }
    else if (MATCH_FIELD("DEFAULT", "region")) { credentials->region = strdup(value); }
    else if (MATCH_FIELD("DEFAULT", "key_file")) {
        if (value[0] == '~') {
            const char *home_path = GET_HOME_PATH();
            size_t new_len = strlen(home_path) + strlen(value + 1) + 1;
            char final_path[new_len];

            snprintf(final_path, new_len, "%s%s", home_path, value + 1);
            credentials->key_file = strdup(final_path);
        } else { credentials->key_file = strdup(value); }
    } else { 
        return 0;  /* unknown section/name, error */
    }

    return 1;
}

int load_creds_from_config(Credential *credentials, const char *str) {    
    char sure_path[50];
    int verified = verify_path(str, sure_path, 50);
    if (verified != 0) {
        LOG_ERROR("Failed to verify config path!");
        return verified;
    }

    size_t final_size = sizeof(sure_path) + sizeof("/config");
    char filename[final_size];

    snprintf(filename, final_size, "%s/config", sure_path);
    int ini_parse_result = ini_parse(filename, handler, credentials);
    if (ini_parse_result < 0) {
        LOG_ERROR("Can't load %s\n", filename);
        return ini_parse_result;
    }

    return 0;
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