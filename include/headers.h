#ifndef HEADERS_H
#define HEADERS_H

#include <stddef.h>

#include "auth.h"
#include "request.h"

int setup_default_headers(HttpClient *client);
int set_header(HttpClient *client, const char *name, const char *value);
int apply_signing_headers(HttpClient *client, SigningRequest *sig_req);

int add_headers_with_body(HttpClient *client, const char *body);
const char* get_header(const HttpClient *client, const char *name);
char* build_auth_and_headers(const char *headers_list, const Credential *creds, const char *encoded_sig);

#endif