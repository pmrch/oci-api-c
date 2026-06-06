#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <sodium/utils.h>
#include <stdlib.h>
#include <string.h>

#include "headers.h"
#include "io.h"
#include "log.h"
#include "signing.h"
#include "helpers.h"

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

static char* string_to_sign(HttpClient *client, const bool is_post, SigningRequest *sig_req, const Timestamp ts) {
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
    char *method_path = malloc(method_path_size);
    if (method_path == NULL) {
        LOG_ERROR("Failed to allocate memory for the (request-target) header");
        return NULL;
    }

    // Duplicated to perform in-place lowercasing
    char *original_method = strdup(sig_req->method);
    to_lowercase(original_method);
    sprintf(method_path, "(request-target): %s %s", original_method, sig_req->path);
    free(original_method);

    // Host string extraction
    char *host_buf = malloc(strlen(sig_req->host) + sizeof("host: ") + 1);
    if (host_buf == NULL) {
        LOG_ERROR("Failed to allocate memory for host header!");
        free(method_path);
        return NULL;
    }

    sprintf(host_buf, "host: %s", sig_req->host);
    char *post_headers = malloc(500);

    if (post_headers == NULL)  {
        LOG_ERROR("Failed to allocate memory for POST headers");
        free(method_path);
        free(host_buf);
        return NULL;
    }

    if (is_post) {
        const char *xcs = get_header(client, "x-content-sha256");
        const char *ct = get_header(client, "content-type");
        const char *cl = get_header(client, "content-length");

        if (xcs && ct && cl) {
            sprintf(post_headers, "x-content-sha256: %s\ncontent-type: %s\ncontent-length: %s", xcs, ct, cl);
            
            size_t all_header_size = strlen(date) + strlen(method_path) + strlen(host_buf) + strlen(post_headers) + 1;
            char *headers_all = malloc(all_header_size);
            if (headers_all == NULL) {
                LOG_ERROR("Failed to allocate memory for full header size");
                free(post_headers);
                return NULL;
            }

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

void free_signing_req(SigningRequest* signing_req) {
    if (signing_req->host) { free(signing_req->host); }
    if (signing_req->path) { free(signing_req->path); }
    if (signing_req->scheme) { free(signing_req->scheme); }
    free(signing_req);
}

int sign_request(HttpClient *http_client, const char *body, Credential *creds) {
    int result = 0;
    const bool is_post = http_client->method == PUT || http_client->method == POST || http_client->method == PATCH;
    add_headers_with_body(http_client, body);

    if (creds == NULL) {
        LOG_ERROR("Credential was NULL in sign_request!");
        return -1;
    }

    const Timestamp now = timestamp_now();
    SigningRequest *signing_req = build_signing_request(http_client);

    // Construct string to sign
    char *str_to_sign = string_to_sign(http_client, is_post, signing_req, now);
    LOG_DEBUG("Body size: %zu", strlen(body));

    // Read private key from file
    char *pem_string = read_to_string(creds->key_file);
    if (pem_string == NULL) { return -1; goto cleanup; }

    EVP_PKEY *private_key = load_pkcs8_from_string(pem_string);
    free(pem_string);

    if (private_key == NULL) { return -1; goto cleanup; }

    // Sign the string
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        LOG_ERROR("Failed to create new MD CTX for OpenSSL");
        goto cleanup;
        return -1;
    }
    
    EVP_PKEY_CTX *pkey_ctx = NULL;
    EVP_DigestSignInit(ctx, &pkey_ctx, EVP_sha256(), NULL, private_key);
    EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING);
    EVP_DigestSignUpdate(ctx, str_to_sign, strlen(str_to_sign));
    free(str_to_sign);

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
        free(sig);
        goto cleanup;
        return -1;
    }

    char *encoded_sig = sodium_bin2base64(b64, b64_len, sig, sig_len, sodium_base64_VARIANT_ORIGINAL);
    if (encoded_sig == NULL) {
        LOG_ERROR("Failed to base64 encode the RSA PEM");
        free(b64);
        free(sig);
        goto cleanup;
        return -1;
    }

    free(sig);
    char *auth_value = build_auth_and_headers(headers_list, creds, encoded_sig);
    if (auth_value == NULL) {
        free(b64);
        free(encoded_sig);
        goto cleanup; 
        return -1;
    }

    int set_authorization = set_header(http_client, "authorization", auth_value);
    free(auth_value);

    int applied_headers = apply_signing_headers(http_client, signing_req);
    free(b64);
    free_signing_req(signing_req);

    if (set_authorization != 0) {
        LOG_ERROR("Failed to set authorization headers!");
        goto cleanup;
        return set_authorization;
    }

    if (applied_headers != 0) {
        LOG_ERROR("Failed to apply signing headers to the client");
        goto cleanup;
        return applied_headers;
    }

    if (ctx) EVP_MD_CTX_free(ctx);
    if (private_key) EVP_PKEY_free(private_key);
    
    return 0;

cleanup:
    if (ctx) EVP_MD_CTX_free(ctx);
    if (private_key) EVP_PKEY_free(private_key);
    EVP_cleanup();

    free_signing_req(signing_req);
    return result;
}