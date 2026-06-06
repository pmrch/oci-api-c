#include <stddef.h>

#include "auth.h"
#include "request.h"

void free_signing_req(SigningRequest* signing_req);
int sign_request(HttpClient *http_client, const char *body, Credential *creds);