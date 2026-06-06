#ifndef HELPERS_H
#define HELPERS_H

#include <stddef.h>
#include <time.h>

#include "app_context.h"

typedef enum {
    AD,
    RETRY
} IntervalType;

typedef struct {
    char* key_type;
    char* key;
} MAYBE_SSH_KEY;

typedef struct {
    struct timespec poll_interval;
    struct timespec ad_interval;
} Intervals;

typedef struct {
    char* code;
    char* message;
    long status;
} Answer;

void free_answer(Answer *ans);
void to_lowercase(char *str);
void update_ad_interval(struct timespec *ts, const size_t num_429);
void update_poll_interval(struct timespec *ts, const size_t num_429);

char* load_env_var(const char *str);
char* build_launch_json(const AppContext *ctx, const char *ad);

Intervals setup_intervals(const long poll_secs, const double ad_secs);
Answer* get_answer(const char *response);

size_t WriteAnswerCallback(void *contents, size_t size, size_t nmemb, void *userp);

#endif