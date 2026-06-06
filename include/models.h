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
    const char* url;
    HttpHeader* headers;
    HttpMethod method;
    size_t header_count;
} HttpClient;

typedef struct {
    size_t header_count;  // already have this
    HttpHeader *headers;  // already have this
    const char *method;   // already have this
    char* scheme;         // from CURLU
    char* host;           // from CURLU
    char* path;           // from CURLU
} SigningRequest;

typedef struct {
    char* memory;
    size_t size;
} MemoryStruct;
