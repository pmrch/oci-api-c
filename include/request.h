#ifndef REQUEST_H
#define REQUEST_H

#include "auth.h"
#include <curl/curl.h>
#include <stddef.h>

typedef enum {
    POST,
    PATCH,
    GET,
    PUT,
    DELETE
} HttpMethod;

typedef struct {
    char* name;
    char* value;
} HttpHeader;

typedef struct {
    CURL* handle;
    const char *url;
    HttpHeader* headers;
    HttpMethod method;
    size_t header_count;
} HttpClient;

typedef struct {
    size_t header_count;  // already have this
    HttpHeader *headers;  // already have this
    const char *method;   // already have this
    char *scheme;         // from CURLU
    char *host;           // from CURLU
    char *path;           // from CURLU
} SigningRequest;

const char* fmt_method(const HttpMethod method);
void free_signing_req(SigningRequest* signing_req);
void free_client(HttpClient *client);

HttpClient* create_client();
int setup_default_headers(HttpClient *client);
struct curl_slist* apply_headers_to_curl(HttpClient *client);
const char* get_header(const HttpClient *client, const char *name);

void sign_request(HttpClient *http_client, const char *body, Credential *creds);

#endif