#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <yyjson.h>
#include <dotenv.h>
#include <curl/curl.h>

#include "helpers.h"
#include "app_context.h"
#include "log.h"
#include "request.h"

static const char *availability_domains[3] = { 
    "fZvm:EU-FRANKFURT-1-AD-1", 
    "fZvm:EU-FRANKFURT-1-AD-2", 
    "fZvm:EU-FRANKFURT-1-AD-3"
};

int main() {
    env_load(".", false);
    AppContext *ctx = setup_app_context(NULL, NULL);
    if (ctx == NULL) { return -1; }

    bool should_be_running = true;
    Intervals intervals = setup_intervals(ctx->res->behaviour->poll_interval_secs, ctx->res->behaviour->ad_interval_secs);
    
    // Get the JSON
    char *json = build_launch_json(availability_domains[0], ctx->credentials, ctx->secrets, ctx->res->resources);
    if (json == NULL) {
        free_app_context(ctx);
        return -1;
    }

    // Setup HTTPS stuff
    const char *url_slice1 = "https://iaas.";
    const char *url_slice2 = ".oraclecloud.com/20160918/instances";
    size_t url_len = (strlen(url_slice1) + strlen(url_slice2) + strlen(ctx->credentials->region) + 1);

    char url[url_len];
    snprintf(url, url_len, "%s%s%s", url_slice1, ctx->credentials->region, url_slice2);
    printf("URL: %s\n", url);

    CURL *http_client = curl_easy_init();
    if (http_client == NULL) {
        LOG_ERROR("Failed to create cURL HTTP client!\n");
        return -1;
    }
    
    curl_easy_setopt(http_client, CURLOPT_URL, url);
    curl_easy_setopt(http_client, CURLOPT_POSTFIELDS, json);
    struct curl_slist *headers = setup_headers(http_client);

    if (headers == NULL || headers->data == NULL) {
        LOG_ERROR("Failed to seup headers!");
        return -2;
    }

    printf("Headers data: %s\n", headers->data);
    while (should_be_running) {
        for (unsigned int i = 0; i <= 2; i++) {
            printf("%s\n", availability_domains[i]);
            nanosleep(&intervals.ad_interval, NULL);
        }

        nanosleep(&intervals.poll_interval, NULL);
    }

    free_app_context(ctx);
    free(json);

    curl_easy_perform(http_client);
    curl_easy_cleanup(http_client);
    curl_slist_free_all(headers);
    return 0;
}