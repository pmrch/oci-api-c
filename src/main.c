#include <stdio.h>
#include <yyjson.h>
#include <dotenv.h>

#include "auth.h"
#include "helpers.h"
#include "resources.h"
#include "secrets.h"

static const char *availability_domains[3] = { 
    "fZvm:EU-FRANKFURT-1-AD-1", 
    "fZvm:EU-FRANKFURT-1-AD-2", 
    "fZvm:EU-FRANKFURT-1-AD-3"
};

int main() {
    env_load(".", false);

    Credential credentials = {0};
    int cred_result = load_creds_from_config(&credentials, NULL);
    if (cred_result != 0) { return cred_result; }

    Secrets secrets = {0};
    int sec_result = new_secrets_from_env(&secrets);
    if (sec_result != 0) { return sec_result; }

    Resources res = { .ocpus=2, .memory_gb=12, .name="ampere-c-instance", .shape="VM.Standard.A1.Flex" };
    const char *json = build_launch_json(availability_domains[0], &credentials, &secrets, &res);
    printf("%s\n", json);

    return 0;
}