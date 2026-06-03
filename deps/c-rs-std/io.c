#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "io.h"
#include "log.h"

long long get_filesize(const char* filename) {
    struct stat st;

    if (stat(filename, &st) == 0) {
        LOG_DEBUG("get_filesize: %s = %zu bytes", filename, st.st_size);
        return st.st_size;
    } else {
        LOG_WARN("get_filesize: failed to stat file %s", filename);
        return -1;
    }
}

int64_t get_filesize_from_handle(FILE* handle, char* name) {
    if (fseek(handle, 0, SEEK_END) != 0) {
        if (name != NULL) {
            LOG_ERROR("Failed to get file size for '%s'", name);
            return -1;
        }

        LOG_ERROR("Failed to get file size from a handle");
        return -1;
    }

    int64_t size = ftell(handle);   
    fseek(handle, 0, SEEK_SET);
    return size;  // ftell already returns -1 on failure
}

int file_open(File *out, const char *path, const char *mode) {
    LOG_DEBUG("Opening file '%s' with mode '%s'", path, mode);
    
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        LOG_ERROR("Provided path was NOT a file or failed to be read!");
        return -1;
    }

    // sep now points to "/config.txt"
    const char* sep1 = strrchr(path, '/');
    const char* sep2 = strrchr(path, '\\');

    const char* sep = sep1 > sep2 ? sep1 : sep2;
    out->name = strdup(sep ? sep + 1 : path);
    out->path = strdup(path);
    out->closed = false;
    out->handle = fopen(path, mode);

    if (out->handle == NULL) {
        LOG_ERROR("Failed to open file '%s'", path);
        return -1;
    }

    LOG_DEBUG("Successfully opened '%s'", path);
    return 0;
}

int file_close(File* in) {
    if (!in || !in->handle || in->closed) {
        LOG_ERROR("Passed argument file was uninitialized or already closed");
        return -1;
    }

    fclose(in->handle);
    
    in->handle = NULL;
    in->closed = true;

    LOG_DEBUG("file_close: '%s' successfully closed", in->path);

    free(in->name);
    free(in->path);
    in->name = NULL;
    in->path = NULL;

    return 0;
}

char* read_to_string(const char* path) {
    if (path == NULL) {
        LOG_ERROR("read_to_string error: No input path was provided or it was NULL!");
        return NULL;
    }

    LOG_DEBUG("Reading entire file '%s' into string", path);
    File file;
    
    if (file_open(&file, path, "rb") != 0) {
        LOG_ERROR("read_to_string error: failed to open file '%s'", path);
        return NULL;
    }

    if (file.handle ==  NULL) {
        LOG_ERROR("Failed to open file '%s'", path);
        return NULL;
    }

    long size = get_filesize_from_handle(file.handle, file.name);
    if (size < 0) {
        LOG_ERROR("File size was negative for '%s'", path);
        file_close(&file);
        return NULL;
    }

    fseek(file.handle, 0, SEEK_SET);  // rewind
    LOG_DEBUG("Rewound file handle for '%s' before reading", file.name);

    LOG_DEBUG("allocating buffer of size %zu", size + 1);
    char* buffer = malloc((size_t)size + 1);

    if (!buffer) {
        LOG_ERROR("read_to_string error: failed to allocate buffer for file '%s'", path);
        file_close(&file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, file.handle);
    if (read != (size_t)size) {
        if (feof(file.handle))
            LOG_ERROR(
                "unexpected EOF while reading %zu bytes from '%s'",
                size, path
            );
        else if (ferror(file.handle))
            LOG_ERROR(
                "IO error while reading %zu bytes from '%s'",
                size, path
            );
        else
            LOG_ERROR(
                "unknown read error (%zu/%zu bytes read) from '%s'",
                read, size, path
            );

        free(buffer);
        file_close(&file);
        return NULL;
    }

    buffer[size] = '\0';
    LOG_DEBUG("successfully read %zu bytes from '%s'", size, path);
    file_close(&file);

    return buffer;
}
