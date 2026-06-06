#ifndef REQUEST_H
#define REQUEST_H

#include <curl/curl.h>
#include <stddef.h>

#include "models.h"

const char* fmt_method(const HttpMethod method);
int setup_client_defaults(HttpClient *client, const char *url);

void setup_and_send_webhook(const char *url);
void free_client(HttpClient *client);

HttpClient* create_client();
struct curl_slist* apply_headers_to_curl(HttpClient *client);

#endif