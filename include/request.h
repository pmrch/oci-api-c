#ifndef REQUEST_H
#define REQUEST_H

#include <curl/curl.h>

struct curl_slist* setup_headers(CURL *client);

#endif