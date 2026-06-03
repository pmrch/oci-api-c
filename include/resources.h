#ifndef RESOURCES_H
#define RESOURCES_H

#include <stdint.h>

typedef struct {
    char* region;
    char* availability_domains[3];
} Config;

typedef struct {
    double ad_interval_secs;
    int poll_interval_secs;
} Behaviour;

typedef struct {
    char* name;
    char* shape;
    uint16_t ocpus;
    uint16_t memory_gb;
} Resources;

typedef struct {
    Config* config;
    Resources* resources;
    Behaviour* behaviour;
} ResourceConfig;

ResourceConfig* load_resources_from_config(const char *path);
void free_resource_config(ResourceConfig *resconf);
void free_resources(Resources *res);
void free_config(Config *cfg);

#endif