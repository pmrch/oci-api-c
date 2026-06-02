#ifndef RESOURCES_H
#define RESOURCES_H

#include <stdint.h>

typedef struct {
    char* name;
    char* shape;
    uint16_t ocpus;
    uint16_t memory_gb;
} Resources;

#endif