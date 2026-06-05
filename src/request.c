#include <curl/urlapi.h>
#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <sodium.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/utils.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <yyjson.h>

#include "request.h"
#include "auth.h"
#include "helpers.h"
#include "io.h"
#include "log.h"
#include "timestamp.h"

#define MAX_HEADERS 16

/// Private functions section
static int set_header(HttpClient *client, char *name, char *value) {
    //LOG_DEBUG("Setting header key '%s' to values '%s'", name, value);
    for (size_t i = 0; i < client->header_count; i++) {
        if (strcmp(client->headers[i].name, name) == 0) {
            if (client->headers[i].value != value) {
                free(client->headers[i].value);
                client->headers[i].value = strdup(value);
            }

            return 0;
        }
    }

    client->headers[client->header_count].name = strdup(name);
    client->headers[client->header_count].value = strdup(value);
    client->header_count++;

    LOG_DEBUG("Header '%s' has been set", name);
    return 0;
}

static EVP_PKEY* load_pkcs8_from_string(const char *pem_data) {
    if (pem_data == NULL) {
        LOG_ERROR("PEM key string was NULL!");
        return NULL;
    }

    // Create read-only memory BIO
    // -1 makes OpenSSL to calculate the length using strlen()
    BIO *bio = BIO_new_mem_buf(pem_data, -1);
    if (!bio) {
        LOG_ERROR("BIO failed to initialize!");
        return NULL;
    }

    // Read private key from the memory BIO
    // NULL arguments: no passwd, no callback, no user
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);

    // Clean up the BIO
    BIO_free(bio);
    return pkey;
}

static int add_headers_with_body(HttpClient *client, const char *body) {
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

    char content_length_parsed[needed];
    snprintf(content_length_parsed, (size_t)needed, "%zu", content_length);

    int e = set_header(client, "x-content-sha256", encoded);
    int t = set_header(client, "content-type", "application/json");
    int c = set_header(client, "content-length", content_length_parsed);
    
    if ((e + t + c) != 0) {
        LOG_ERROR("Failed to set headers!");
        return -2;
    }

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

static const char* string_to_sign(HttpClient *client, const bool is_post, SigningRequest *sig_req, const Timestamp ts) {
    if (sig_req == NULL) {
        LOG_ERROR("SigningRequest was not properly initialized!");
        return NULL;
    }

    // Get HTTP formatted time first
    char date_buf[34];
    timestamp_format_http_date(ts, date_buf, sizeof(date_buf));
    LOG_DEBUG("Given date buffer of size %zu, read %zu", sizeof(date_buf), strlen(date_buf) + 1);

    char date[40];
    sprintf(date, "date: %s", date_buf);

    // Get HTTP method and path
    size_t method_path_size = (sizeof("(request-target): ") + strlen(sig_req->method) + strlen(sig_req->path) + 5);
    char method_path[method_path_size];

    // Duplicated to perform in-place lowercasing
    char *original_method = strdup(sig_req->method);
    to_lowercase(original_method);
    sprintf(method_path, "(request-target): %s %s", original_method, sig_req->path);
    free(original_method);

    // Host string extraction
    char host_buf[strlen(sig_req->host) + sizeof("host: ") + 1];
    sprintf(host_buf, "host: %s", sig_req->host);

    char *post_headers = malloc(500);
    if (post_headers == NULL)  {
        LOG_ERROR("Failed to allocate memory for POST headers");
        return NULL;
    }

    if (is_post) {
        const char *xcs = get_header(client, "x-content-sha256");
        const char *ct = get_header(client, "content-type");
        const char *cl = get_header(client, "content-length");

        if (xcs && ct && cl) {
            sprintf(post_headers, "x-content-sha256: %s\ncontent-type: %s\ncontent-length: %s", xcs, ct, cl);
            
            size_t all_header_size = sizeof(date) + sizeof(method_path) + sizeof(host_buf) + strlen(post_headers) + 1;
            char headers_all[all_header_size];
            sprintf(headers_all, "%s\n%s\n%s\n%s", date, method_path, host_buf, post_headers);

            free(post_headers);
            return strdup(headers_all);
        } else {
            free(post_headers);
            LOG_ERROR("Failed to get required headers!");
            return NULL;
        }
    }

    size_t all_header_size = sizeof(date) + sizeof(method_path) + sizeof(host_buf);
    char *final_headers = malloc(all_header_size);
    if (final_headers == NULL) {
        LOG_ERROR("Failed to allocate final headers!");
        free(post_headers);
        return NULL;
    }
    snprintf(final_headers, all_header_size, "%s\n%s\n%s", date, method_path, host_buf);
    return final_headers;
}

static SigningRequest* build_signing_request(const HttpClient *client) {
    SigningRequest *sig_req = calloc(1, sizeof(SigningRequest));
    if (sig_req == NULL) {
        LOG_ERROR("Failed to allocate SigningRequest!");
        return NULL;
    }
    
    //Set the URL to be parsed
    CURLU *h = curl_url();
    if (h == NULL) {
        LOG_ERROR("Failed to allocate URL object of CURLU");
        free_signing_req(sig_req);
        return NULL;
    }
    
    LOG_DEBUG("URL is: %s\n", client->url);
    CURLUcode rc = curl_url_set(h, CURLUPART_URL, client->url, 0);
    if (rc != CURLUE_OK) {
        LOG_ERROR("Invalid URL provided to CURLU");
        curl_url_cleanup(h);
        free_signing_req(sig_req);
        return NULL;
    }

    char *scheme, *authority, *path;

    curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);
    curl_url_get(h, CURLUPART_HOST, &authority, 0);
    curl_url_get(h, CURLUPART_PATH, &path, 0);

    sig_req->scheme = strdup(scheme);
    sig_req->host = strdup(authority);
    sig_req->path = strdup(path);

    curl_free(scheme);
    curl_free(authority);
    curl_free(path);
    curl_url_cleanup(h);

    if (sig_req->scheme == NULL || sig_req->host == NULL || sig_req->path == NULL) {
        LOG_ERROR("Failed to allocate URL components");
        free_signing_req(sig_req);
        curl_url_cleanup(h);
        return NULL;
    }

    LOG_DEBUG("scheme: %s | host: %s | path: %s", sig_req->scheme, sig_req->host, sig_req->path);

    sig_req->method = fmt_method(client->method);
    sig_req->headers = client->headers;
    sig_req->header_count = client->header_count;
    return sig_req;
}

static char* build_auth_and_headers(const char *headers_list, const Credential *creds, const char *encoded_sig) {
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

static void apply_signing_request(HttpClient *client, SigningRequest *sig_req) {
    for (size_t i = 0; i < sig_req->header_count; i++) {
        set_header(client, sig_req->headers[i].name, sig_req->headers[i].value);
    }
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

void sign_request(HttpClient *http_client, const char *body, Credential *creds) {
    const bool is_post = http_client->method == PUT || http_client->method == POST || http_client->method == PATCH;
    add_headers_with_body(http_client, body);

    if (creds == NULL) {
        LOG_ERROR("Credential was NULL in sign_request!");
        return;
    }

    const Timestamp now = timestamp_now();
    SigningRequest *signing_req = build_signing_request(http_client);

    // Construct string to sign
    const char *str_to_sign = string_to_sign(http_client, is_post, signing_req, now);
    LOG_DEBUG("Body size: %zu", strlen(body));

    // Read private key from file
    const char *pem_string = read_to_string(creds->key_file);
    if (pem_string == NULL) { return; }

    EVP_PKEY *private_key = load_pkcs8_from_string(pem_string);
    if (private_key == NULL) { return; }

    // Sign the string
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        LOG_ERROR("Failed to create new MD CTX for OpenSSL");
        return;
    }

    
    EVP_PKEY_CTX *pkey_ctx = NULL;
    EVP_DigestSignInit(ctx, &pkey_ctx, EVP_sha256(), NULL, private_key);
    EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING);
    EVP_DigestSignUpdate(ctx, str_to_sign, strlen(str_to_sign));

    size_t sig_len = 0;
    EVP_DigestSignFinal(ctx, NULL, &sig_len);
    unsigned char *sig = malloc(sig_len);
    EVP_DigestSignFinal(ctx, sig, &sig_len);

    // Set headers
    char now_str[31];
    timestamp_format_http_date(now, now_str, sizeof(now_str));
    set_header(http_client, "date", now_str);

    // Build the authorization header
    const char *headers_list = NULL;
    if (is_post) {
        headers_list = "date (request-target) host x-content-sha256 content-type content-length";
    } else {
        headers_list = "date (request-target) host";
    };

    size_t b64_len = sodium_base64_ENCODED_LEN(sig_len, sodium_base64_VARIANT_ORIGINAL) + 1;
    char *b64 = malloc(b64_len);
    if (b64 == NULL) {
        LOG_ERROR("Failed to allocate memory for b64!");
        return;
    }

    const char *encoded_sig = sodium_bin2base64(b64, b64_len, sig, sig_len, sodium_base64_VARIANT_ORIGINAL);
    if (encoded_sig == NULL) {
        LOG_ERROR("Failed to base64 encode the RSA PEM");
        free(sig);
        return;
    }

    char *auth_value = build_auth_and_headers(headers_list, creds, encoded_sig);
    if (auth_value == NULL) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(private_key);    
        return;
    }

    set_header(http_client, "authorization", auth_value);
    apply_signing_request(http_client, signing_req);
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(private_key);
}

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
            free((char *)client->headers[i].value);
        }
        
        free(client->headers); 
    }
    free(client);
}

void free_signing_req(SigningRequest* signing_req) {
    if (signing_req->host) { free(signing_req->host); }
    if (signing_req->path) { free(signing_req->path); }
    if (signing_req->headers) { free(signing_req->headers); }
    free(signing_req);
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