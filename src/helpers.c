#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

#include "helpers.h"
#include "app_context.h"
#include "auth.h"
#include "log.h"
#include "resources.h"
#include "secrets.h"

char* strip_quotes(char *str) {
    if (str == NULL) return NULL;
    
    size_t len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len-1] == '"') {
        str[len-1] = '\0';
        memmove(str, str + 1, len - 1);  // shift left in place
    }
    
    return str;
}

char* load_env_var(const char *var_name) {
    char *value = getenv(var_name);
    char *copy = strdup(value);

    char *key_type = strtok(copy, " "); // Get the key type
    char *key_data = strtok(NULL, ""); // Get the remainder of the string

    MAYBE_SSH_KEY maybe_ssh_key = { .key = NULL, .key_type = NULL };
    if (key_type != NULL && key_data != NULL) {
        maybe_ssh_key.key_type = key_type;
        maybe_ssh_key.key = key_data;
    } else {
        free(copy);
        return strip_quotes(strdup(value));
    }

    if (maybe_ssh_key.key_type && maybe_ssh_key.key_type[0] == '"') {
        maybe_ssh_key.key_type++;
    }

    size_t len = strlen(maybe_ssh_key.key);
    if (maybe_ssh_key.key[len-1] == '"') {
        maybe_ssh_key.key[len-1] = '\0';
    }

    size_t type_len = strlen(maybe_ssh_key.key_type);
    size_t data_len = strlen(maybe_ssh_key.key);

    char *buf = malloc(type_len + data_len + 2);
    memcpy(buf, maybe_ssh_key.key_type, type_len);

    buf[type_len] = ' ';
    memcpy(buf + type_len + 1, maybe_ssh_key.key, data_len);
    buf[type_len + data_len + 1] = '\0';

    free(copy);
    return buf;
}

char* build_launch_json(const AppContext *ctx, const char *ad) {
    const Resources *res = ctx->res->resources;
    const Credential *creds = ctx->credentials;
    const Secrets *secrets = ctx->secrets;

    if (secrets == NULL || secrets->image_id == NULL || secrets->ssh_key == NULL || secrets->subnet_id == NULL) {
        LOG_ERROR("Secrets was not initialized, or had missing fields!");
        return NULL;
    }

    if (res == NULL || res->name == NULL || res->shape == NULL) {
        LOG_ERROR("Resources was not initialized or had missing fields!");
        return NULL;
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    // Top-level fields
    yyjson_mut_obj_add_str(doc, root, "availabilityDomain", ad);
    yyjson_mut_obj_add_str(doc, root, "compartmentId", creds->tenancy);
    yyjson_mut_obj_add_str(doc, root, "displayName", res->name);
    yyjson_mut_obj_add_str(doc, root, "shape", res->shape);
    yyjson_mut_obj_add_bool(doc, root, "is_pv_encryption_in_transit_enabled", true) ;

    // shapeConfig
    yyjson_mut_val *shape_config = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, shape_config, "ocpus", res->ocpus);
    yyjson_mut_obj_add_int(doc, shape_config, "memoryInGbs", res->memory_gb);
    yyjson_mut_obj_add_val(doc, root, "shapeConfig", shape_config);

    // sourceDetails
    yyjson_mut_val *source_details = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, source_details, "sourceType", "image");
    yyjson_mut_obj_add_str(doc, source_details, "imageId", secrets->image_id);
    yyjson_mut_obj_add_val(doc, root, "sourceDetails", source_details);

    // createVnicDetails
    yyjson_mut_val *create_vnic_details = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_bool(doc, create_vnic_details, "assignPublicIp", true);
    yyjson_mut_obj_add_bool(doc, create_vnic_details, "assignPrivateDnsRecord", true);
    yyjson_mut_obj_add_str(doc, create_vnic_details, "subnetId", secrets->subnet_id);
    yyjson_mut_obj_add_val(doc, root, "createVnicDetails", create_vnic_details);

    // metadata
    yyjson_mut_val *metadata = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, metadata, "ssh_authorized_keys", secrets->ssh_key);
    yyjson_mut_obj_add_val(doc, root, "metadata", metadata);

    // availabilityConfig
    yyjson_mut_val *availability_config = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, availability_config, "recoveryAction", "RESTORE_INSTANCE");
    yyjson_mut_obj_add_val(doc, root, "availabilityConfig", availability_config);
    yyjson_mut_obj_add_str(doc, root, "subnetId", secrets->subnet_id);

    // construct the JSON string
    char *json = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    return json;
}


/*size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0; // out of memory!

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0; // Ensure null-termination

    return realsize;
}*/

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;

    // 1. Free the existing memory to prevent leaks
    free(mem->memory);

    // 2. Allocate exactly enough space for the new content + null terminator
    mem->memory = malloc(realsize + 1);
    
    if (mem->memory == NULL) {
        mem->size = 0; // Reset size on failure
        return 0;      // Out of memory
    }

    // 3. Copy the new data over
    memcpy(mem->memory, contents, realsize);
    
    // 4. Update the tracker to reflect only the new size
    mem->size = realsize;
    mem->memory[mem->size] = 0; // Ensure null-termination

    return realsize;
}

Intervals setup_intervals(const long poll_secs, const double ad_secs) {
    Intervals intervals = {0};

    intervals.ad_interval.tv_sec = 0;
    intervals.ad_interval.tv_nsec = (long)(1e9 * ad_secs);

    intervals.poll_interval.tv_sec = poll_secs;
    intervals.poll_interval.tv_nsec = 0;

    return intervals;
}

void to_lowercase(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}