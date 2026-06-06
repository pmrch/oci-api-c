#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    FILE* handle;
    char* name;
    char* path;
    bool closed;
} File;

int file_open(File *out, const char *path, const char *mode);
int file_close(File *in);

int64_t get_filesize_from_handle(FILE* handle, char* name);
long long get_filesize(const char *filename);
char* read_to_string(const char* path);

#endif