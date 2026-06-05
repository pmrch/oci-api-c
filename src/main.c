#include <sodium/core.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sodium.h>
#include <yyjson.h>
#include <dotenv.h>
#include <curl/curl.h>

#include "auth.h"
#include "helpers.h"
#include "app_context.h"
#include "log.h"
#include "request.h"
#include "resources.h"

typedef struct {
    const char* code;
    const char* message;
} Answer;

static Answer* get_answer(const char *response) {
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
        yyjson_doc_free(doc);
        return NULL;
    }

    if (code_str == NULL) { ans->code = "<NO CODE>"; }
    else { ans->code = strdup(code_str); }

    if (message_str == NULL) { ans->message = "<NO MESSAGE>"; }
    else { ans->message = strdup(message_str); }

    yyjson_doc_free(doc);
    return ans;
}

void format_with_jq(const char *json_string) {
    // Open a pipe to the 'jq' command
    // We use "jq ." which tells jq to read from stdin and format/pretty-print it
    FILE *pipe = popen("jq .", "w");

    if (pipe == NULL) {
        perror("Failed to run jq");
        return;
    }

    // Write your JSON string into the pipe
    fprintf(pipe, "%s", json_string);

    // Close the pipe and wait for jq to finish
    pclose(pipe);
}

int main() {
    int sod_res = sodium_init();
    if (sod_res != 0) {
        LOG_ERROR("Failed to initialize libsodium crypto library!");
        return sod_res;
    }

    env_load(".", false);
    AppContext *ctx = setup_app_context(NULL, NULL);
    if (ctx == NULL) { return -1; }

    bool should_be_running = true;
    Intervals intervals = setup_intervals(ctx->res->behaviour->poll_interval_secs, ctx->res->behaviour->ad_interval_secs);

    // Setup HTTPS stuff
    const char *url_slice1 = "https://iaas.";
    const char *url_slice2 = ".oraclecloud.com/20160918/instances";
    size_t url_len = (strlen(url_slice1) + strlen(url_slice2) + strlen(ctx->credentials->region) + 1);

    char url[url_len];
    sprintf(url, "%s%s%s", url_slice1, ctx->credentials->region, url_slice2);

    // Create the cURL client
    HttpClient *http_client = create_client();
    if (http_client == NULL) {
        LOG_ERROR("Failed to create cURL HTTP client!\n");
        return -1;
    }
    
    setup_default_headers(http_client);

    // Set the URL (this is glboal)
    http_client->url = url;
    curl_easy_setopt(http_client->handle, CURLOPT_URL, url);

    MemoryStruct chunk = {malloc(1), 0};
    curl_easy_setopt(http_client->handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(http_client->handle, CURLOPT_WRITEDATA, (void*)&chunk);

    // Setup default HTTP headers
    int successful_setup = setup_default_headers(http_client);
    if (successful_setup != 0) { return -2; }

    while (should_be_running) {
        for (unsigned int i = 0; i <= 2; i++) {
            // Get the JSON
            char *json = build_launch_json(ctx, ctx->res->config->availability_domains[i]);
            if (json == NULL) {
                free_app_context(ctx);
                return -1;
            }

            //format_with_jq(json);
            curl_easy_setopt(http_client->handle, CURLOPT_POSTFIELDS, json);
            sign_request(http_client, json, ctx->credentials);
            struct curl_slist *headers = apply_headers_to_curl(http_client);
            LOG_DEBUG("Successfully signed request!");

            curl_easy_perform(http_client->handle);
            Answer *ans = get_answer(chunk.memory);
            if (ans && ans->message) { LOG_INFO("Server said: %s", ans->message); }

            nanosleep(&intervals.ad_interval, NULL);
            curl_slist_free_all(headers);
            free(json);
        }

        nanosleep(&intervals.poll_interval, NULL);
    }

    free_app_context(ctx);
    curl_easy_cleanup(http_client);
    return 0;
}

/*
aDnPNTdzoXwOkcs3WMn51yBOOuJbng/yuF74TS1k0vsXblGj0115GP/U21JhuF2v5UHBzW/AAM/XTWNbAQt5TOOwudUiIry8RUS3hzdJaQsGIKsACC3oWsGFUgs+95i2kCvfRGJrgqe+guIgjsg0NvnAwaOSQ5Kpc/QC1qiDJo1BLNJ3npU8MSRSYZCTXGCuaO2OSyVtCZxu8lYb2MMqjRmzUiEy2ko+h0o/spW6w5WdmMLPohhs0WYmtZ0mhHy4Q/UDjw2P2rGEkBBTM6oo+403AJmr5CE+anxbujjJmlCkVyXdxtF48INiXkMgbcURxCOAkDpMJ8Ew5xejRbcvGg=="
*/