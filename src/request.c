#include <curl/curl.h>
#include <stdlib.h>

#include "request.h"
#include "log.h"

struct curl_slist* setup_headers(CURL *client) {
    struct curl_slist *headers = NULL; // Initialize to NULL
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (headers == NULL) {
        LOG_ERROR("Failed to allocate headers!");
        return NULL;
    }

    curl_easy_setopt(client, CURLOPT_HTTPHEADER, headers);
    return headers;
}