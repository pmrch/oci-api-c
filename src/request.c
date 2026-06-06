#include <curl/urlapi.h>
#include <curl/curl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "request.h"
#include "headers.h"
#include "log.h"


const char* fmt_method(const HttpMethod method) {
    switch (method) {
        case GET: return "GET";
        case POST: return "POST";
        case PATCH: return "PATCH";
        case PUT: return "PUT";
        case DELETE:return "DELETE";
        default: {
            LOG_ERROR("Unknown HTTP method!");
            return NULL;
        }
    }
}

void free_client(HttpClient *client) {
    if (client->handle) { curl_easy_cleanup(client->handle); }
    if (client->headers) { 
        for (size_t i = 0; i < client->header_count; i++) {
            // only free values that were strdup'd, not string literals
            free(client->headers[i].name);
            free(client->headers[i].value);
        }
        
        free(client->headers); 
    }
    free(client);
}

// Don't forget, returned client must be free()'d!
HttpClient* create_client() {
    HttpClient *client = calloc(1, sizeof(HttpClient));
    if (client == NULL) {
        LOG_ERROR("Failed to allocate HttpClient!");
        return NULL;
    }

    CURL *handle = curl_easy_init();
    if (handle == NULL) {
        LOG_ERROR("Failed to create cURL handle!");
        free_client(client);
        return NULL;
    }

    client->handle = handle;
    client->method = POST;

    return client;
}

int setup_client_defaults(HttpClient *client, const char *url) {
    client->url = url;
    curl_easy_setopt(client->handle, CURLOPT_URL, url);

    // Setup default HTTP headers
    int successful_setup = setup_default_headers(client);
    if (successful_setup != 0) { return -2; }

    return 0;
}