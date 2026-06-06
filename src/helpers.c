#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yyjson.h>

#include "helpers.h"
#include "app_context.h"
#include "auth.h"
#include "log.h"
#include "models.h"
#include "resources.h"
#include "secrets.h"

#define LOWEST_AD_RETRY (334 * 1e6)
#define LOWEST_POLL_RETRY 10
#define HIGHEST_AD_RETRY (720 * 1e6)
#define HIGHEST_POLL_RETRY 60


static void clamp_ad_retry(double *current_interval_ns) {
    if (*current_interval_ns > HIGHEST_AD_RETRY) {
        *current_interval_ns = HIGHEST_AD_RETRY;
    } else if (*current_interval_ns < LOWEST_AD_RETRY) {
        *current_interval_ns = LOWEST_AD_RETRY;
    }
}

static void clamp_poll_retry(double *current_interval_s) {
    if (*current_interval_s > HIGHEST_POLL_RETRY) {
        *current_interval_s = HIGHEST_POLL_RETRY;
    } else if (*current_interval_s < LOWEST_POLL_RETRY) {
        *current_interval_s = LOWEST_POLL_RETRY;
    }
}

static double random_range(double min, double max) {
    return min + ((double)rand() / RAND_MAX) * (max - min);
}

static char* strip_quotes(char *str) {
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

/// contents is what we get from the server (char *)
/// size is always 1
/// nmemb is size of object in bytes
/// userp is the pointer you give CURLOPT_WRITEDATA
size_t WriteAnswerCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Answer *ans = (Answer *)userp;

    // Extract contents as string
    char *data = malloc(realsize + 1);
    if (data == NULL) {
        LOG_ERROR("Server returned NULL on callback");
        return 0;      // Out of memory
    }

    memcpy(data, contents, realsize);
    data[realsize] = '\0';

    // Convert contents into Answer
    if (ans == NULL) { 
        LOG_ERROR("Passed in a NULL for the target of cURL callback");
        free(data);
        return 0; 
    }

    Answer *target_ans = get_answer(data);
    if (target_ans == NULL) {
        free(data);
        LOG_ERROR("Failed to get answer from raw contents");
        return 0; 
    }

    free(data);
    if (target_ans->code) { ans->code = strdup(target_ans->code); }
    if (target_ans->message) { ans->message = strdup(target_ans->message); }

    free_answer(target_ans);
    return realsize;
}

Answer* get_answer(const char *response) {
    yyjson_doc *doc = yyjson_read(response, strlen(response), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL) {
        LOG_ERROR("Response was not a valid JSON! %s", response);
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *code = yyjson_obj_get(root, "code");
    const char *code_str = yyjson_get_str(code);

    yyjson_val *message = yyjson_obj_get(root, "message");
    const char *message_str = yyjson_get_str(message);

    Answer *ans = calloc(1, sizeof(Answer));
    if (ans == NULL) {
        LOG_ERROR("Failed to allocate memory for Answer in get_answer");
        yyjson_doc_free(doc);
        return NULL;
    }

    if (code_str == NULL) { ans->code = strdup("<NO CODE>"); }
    else { ans->code = strdup(code_str); }

    if (message_str == NULL) { ans->message = strdup("<NO MESSAGE>"); }
    else { ans->message = strdup(message_str); }

    yyjson_doc_free(doc);
    return ans;
}

void free_answer(Answer *ans) {
    if (ans->code) { free(ans->code); }
    if (ans->message) { free(ans->message); }
    free(ans);
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

void update_ad_interval(struct timespec *ts, const size_t num_429) {
    if (num_429 >= 2) {
        double new_rand = random_range(2.0, 2.5);
        double new_interval = (double)ts->tv_nsec * new_rand;
        clamp_ad_retry(&new_interval);

        ts->tv_nsec = (long)new_interval;
    } else if (num_429 >= 1) {
        double new_rand = random_range(1.45, 1.75);
        double new_interval = (double)ts->tv_nsec * new_rand;
        clamp_ad_retry(&new_interval);

        ts->tv_nsec = (long)new_interval;
    } else {
        double new_rand = random_range(0.75, 0.9);
        double new_interval = (double)ts->tv_nsec * new_rand;
        clamp_ad_retry(&new_interval);

        ts->tv_nsec = (long)new_interval;
    }
}

void update_poll_interval(struct timespec *ts, const size_t num_429) {
    if (num_429 >= 2) {
        double new_rand = random_range(1.35, 1.45);
        double new_interval = (double)ts->tv_sec * new_rand;
        clamp_poll_retry(&new_interval);

        ts->tv_sec = (long)(new_interval);
    } else if (num_429 >= 1) {
        double new_rand = random_range(1.0, 1.25);
        double new_interval = (double)ts->tv_sec * new_rand;
        clamp_poll_retry(&new_interval);

        ts->tv_sec = (long)(new_interval);
    } else {
        double new_rand = random_range(0.65, 0.85);
        double new_interval = (double)ts->tv_sec * new_rand;
        clamp_poll_retry(&new_interval);

        ts->tv_sec = (long)(new_interval);
    }
}