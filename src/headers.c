#include <sodium/crypto_hash_sha256.h>
#include <sodium/utils.h>
#include <stddef.h>
#include <stdlib.h>

#include "headers.h"
#include "log.h"
#include "request.h"

#define MAX_HEADERS 16

int set_header(HttpClient *client, const char *name, const char *value) {
    //LOG_DEBUG("Setting header key '%s' to values '%s'", name, value);
    for (size_t i = 0; i < client->header_count; i++) {
        if (strcmp(client->headers[i].name, name) == 0) {
            if (client->headers[i].value != value) {
                char *new_value = strdup(value);
                if (!new_value) { return -1; }

                free(client->headers[i].value);
                client->headers[i].value = new_value;
            }

            return 0;
        }
    }

    char *new_name = strdup(name);
    if (!new_name) { return -1; }

    char *new_value = strdup(value);
    if (!new_value) {
        free(new_name);
        return -1;
    }

    client->headers[client->header_count].name = new_name;
    client->headers[client->header_count].value = new_value;
    client->header_count++;

    LOG_DEBUG("Header '%s' has been set", name);
    return 0;
}

const char* get_header(const HttpClient *client, const char *name) {
    for (size_t i = 0; i < client->header_count; i++) {
        if (strcmp(client->headers[i].name, name) == 0) {
            return client->headers[i].value;
        }
    }

    LOG_ERROR("Failed to get required header: %s", name);
    return NULL;
}

char* build_auth_and_headers(const char *headers_list, const Credential *creds, const char *encoded_sig) {
    size_t auth_size = strlen(creds->tenancy) + strlen(creds->user) 
        + strlen(creds->fingerprint) + strlen(headers_list) + strlen(encoded_sig) + 128;

    char *auth_value = malloc(auth_size);
    if (auth_value == NULL) {
        LOG_ERROR("Failed to allocate memory for auth value");
        return NULL;
    }

    snprintf(auth_value, auth_size, "Signature version=\"1\",headers=\"%s\",keyId=\"%s/%s/%s\",algorithm=\"rsa-sha256\",signature=\"%s\"",
        headers_list, creds->tenancy, creds->user, creds->fingerprint, encoded_sig);

    return auth_value;
}

int add_headers_with_body(HttpClient *client, const char *body) {
    if (client == NULL) {
        LOG_ERROR("Client was uninitialized at the point of adding body headers");
        return -2;
    }

    if (body == NULL) {
        LOG_ERROR("Body was NULL while adding headers!");
        return -2;
    }

    const unsigned char* body_prepped = (const unsigned char *)body;
    unsigned char hash[crypto_hash_sha256_BYTES];
    int success = crypto_hash_sha256(hash, body_prepped, strlen(body));
    if (success != 0) {
        LOG_ERROR("Failed to encrypt the body with SHA256");
        return success;
    }

    char b64[sodium_base64_ENCODED_LEN(crypto_hash_sha256_BYTES, sodium_base64_VARIANT_ORIGINAL) + 1];
    char *encoded = sodium_bin2base64(b64, sizeof(b64), hash, crypto_hash_sha256_BYTES, sodium_base64_VARIANT_ORIGINAL);
    if (encoded == NULL) {
        LOG_ERROR("Failed to encode the SHA256 into base64");
        return -2;
    }

    size_t content_length = strlen(body);
    int needed = snprintf(NULL, 0, "%zu", content_length) + 1;

    char *content_length_parsed = malloc((size_t)needed);
    if (content_length_parsed == NULL) {
        LOG_ERROR("Failed to allocate memory for parsed content of request");
        return -2;
    }

    snprintf(content_length_parsed, (size_t)needed, "%zu", content_length);
    int e = set_header(client, "x-content-sha256", encoded);
    int t = set_header(client, "content-type", "application/json");
    int c = set_header(client, "content-length", content_length_parsed);
    free(content_length_parsed);
    
    if ((e + t + c) != 0) {
        LOG_ERROR("Failed to set headers!");
        return -2;
    }

    return 0;
}

int apply_signing_headers(HttpClient *client, SigningRequest *sig_req) {
    for (size_t i = 0; i < sig_req->header_count; i++) {
        int header_result = set_header(client, sig_req->headers[i].name, sig_req->headers[i].value);
        if (header_result != 0) {
            LOG_ERROR("Failed to ");
            return -1;
        }
    }

    return 0;
}

int setup_default_headers(HttpClient *client) {
    if (client->headers == NULL) {
        client->headers = calloc(MAX_HEADERS, sizeof(HttpHeader));
        if (client->headers == NULL) {
            LOG_ERROR("Failed to allocate headers!");
            return -1;
        }
    }

    set_header(client, "content-type", "application/json");
    return 0;
}


struct curl_slist* apply_headers_to_curl(HttpClient *client) {
    struct curl_slist *headers = NULL;
    for (size_t i = 0; i < client->header_count; i++) {
        char buf[2048];
        snprintf(buf, sizeof(buf), "%s: %s", client->headers[i].name, client->headers[i].value);

        LOG_DEBUG("Inserting into curl_slist: key '%s': value '%s'", 
            client->headers[i].name, client->headers[i].value
        );

        headers = curl_slist_append(headers, buf);
        if (headers == NULL) {
            LOG_ERROR("Failed to allocate headers!");
            return NULL;
        }
    }

    curl_easy_setopt(client->handle, CURLOPT_HTTPHEADER, headers);
    return headers;
}