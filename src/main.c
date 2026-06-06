#include <sodium.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <yyjson.h>
#include <dotenv.h>
#include <curl/curl.h>

#include "auth.h"
#include "helpers.h"
#include "app_context.h"
#include "log.h"
#include "request.h"
#include "signing.h"
#include "resources.h"

#ifdef PRINT_JSON_PRETTY
static void format_with_jq(const char *json_string) {
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
#endif

int main() {
    srand((unsigned int)time(NULL));

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

    // Build the URL
    const char *url_slice1 = "https://iaas.";
    const char *url_slice2 = ".oraclecloud.com/20160918/instances";
    size_t url_len = (strlen(url_slice1) + strlen(url_slice2) + strlen(ctx->credentials->region) + 1);

    char *url = malloc(url_len);
    if (url == NULL) {
        LOG_ERROR("Failed to allocate memory for final URL");
        free_app_context(ctx);
        return -1;
    }

    sprintf(url, "%s%s%s", url_slice1, ctx->credentials->region, url_slice2);

    // Create the cURL client
    HttpClient *http_client = create_client();
    if (http_client == NULL) {
        free_app_context(ctx);
        LOG_ERROR("Failed to create cURL HTTP client!\n");
        return -1;
    }

    // Set the URL (this is glboal)
    int client_defaults_set = setup_client_defaults(http_client, url);
    if (client_defaults_set != 0) {
        LOG_ERROR("Failed to set client defaults!");
        free_app_context(ctx);
        free_client(http_client);
        return client_defaults_set;
    }

    size_t oo = 0;
    while (should_be_running && oo <= 2) {
        size_t num_429 = 0;
        for (unsigned int i = 0; i <= 2; i++) {
            // Get the JSON
            const char *ad = ctx->res->config->availability_domains[i];
            char *json = build_launch_json(ctx, ad);
            if (json == NULL) {
                free_client(http_client);
                free_app_context(ctx);
                return -1;
            }

            //format_with_jq(json);
            curl_easy_setopt(http_client->handle, CURLOPT_POSTFIELDS, json);
            int signed_req = sign_request(http_client, json, ctx->credentials);
            if (signed_req != 0) { 
                LOG_WARN("Failed to sign request! Skipping iteration");
                free(json);
                continue;
            }

            struct curl_slist *headers = apply_headers_to_curl(http_client);
            LOG_DEBUG("Successfully signed request!");

            Answer ans = {0};
            curl_easy_setopt(http_client->handle, CURLOPT_WRITEFUNCTION, WriteAnswerCallback);
            curl_easy_setopt(http_client->handle, CURLOPT_WRITEDATA, &ans);
            curl_easy_perform(http_client->handle);

            long http_code;
            curl_easy_getinfo(http_client->handle, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 429) { num_429++; }

            ans.status = http_code;
            LOG_WARN("Error in %s: status=%zu code=%s message=%s", ad, ans.status, ans.code, ans.message); 
            free(ans.code);
            free(ans.message);

            nanosleep(&intervals.ad_interval, NULL);
            curl_slist_free_all(headers);
            free(json);
        }

        update_ad_interval(&intervals.ad_interval, num_429);
        update_poll_interval(&intervals.poll_interval, num_429);

        LOG_INFO("All ADs exhausted, retrying in %lus | updated ad retry to %.3fms", 
            intervals.poll_interval.tv_sec, ((double)intervals.ad_interval.tv_nsec / 1e6)
        );

        oo++;
        nanosleep(&intervals.poll_interval, NULL);
    }

    free_app_context(ctx);
    free_client(http_client);
    curl_easy_cleanup(http_client);
    return 0;
}