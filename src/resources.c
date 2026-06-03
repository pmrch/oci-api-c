#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/limits.h>

#include "resources.h"
#include "log.h"
#include "io.h"

static Config *construct_config(yyjson_val *config) {
    if (config == NULL) {
        LOG_ERROR("Failed to read the 'config' section of the JSON body!");
        return NULL;
    }

    Config *cfg = calloc(1, sizeof(Config));
    const char *region = yyjson_get_str(yyjson_obj_get(config, "region"));
    if (region == NULL) {
        LOG_ERROR("Failed to read the 'region' field of the 'config' section in the JSON!");
        free_config(cfg);
        free(cfg);
        return NULL;    
    }

    cfg->region = strdup(region);
    yyjson_val *availability_domains = yyjson_obj_get(config, "availability_domains");
    if (availability_domains == NULL) {
        LOG_ERROR("Failed to read the 'availability_domains' field of the 'config' section in the JSON!");
        free_config(cfg);
        free(cfg);
        return NULL;
    }

    if (yyjson_is_arr(availability_domains)) {
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(availability_domains, &iter);

        yyjson_val *val;
        size_t iter_index = 0;
        while ((val = yyjson_arr_iter_next(&iter))) {
            const char *reading = yyjson_get_str(val);
            if (!reading || iter_index > 2) {
                printf("End of iteration!");
                break;
            }

            cfg->availability_domains[iter_index] = strdup(reading);
            iter_index++;
        }
    }

    return cfg;
}

static Resources *construct_resources(yyjson_val *resources) {
    if (resources == NULL) {
        LOG_ERROR("Failed to read the 'resources' section of the JSON body!");
        return NULL;
    }

    Resources *res = calloc(1, sizeof(Resources));
    const char *shape = yyjson_get_str(yyjson_obj_get(resources, "shape"));
    if (shape == NULL) {
        LOG_ERROR("Failed to read the 'shape' field of the 'resources' section in the JSON!");
        free_resources(res);
        free(res);
        return NULL;
    }

    const char *display_name = yyjson_get_str(yyjson_obj_get(resources, "name"));
    if (display_name == NULL) {
        LOG_ERROR("Failed to read the 'name' field of the 'resources' section in the JSON!");
        free_resources(res);
        free(res);
        return NULL;
    }

    res->shape = strdup(shape);
    res->name = strdup(display_name);

    const uint16_t ocpus = (uint16_t)yyjson_get_int(yyjson_obj_get(resources, "ocpus"));
    if (ocpus == 0) {
        LOG_ERROR("Failed to read the 'ocpus' field of the 'resources' section in the JSON!");
        return NULL;
    }

    const uint16_t memory_in_gbs = (uint16_t)yyjson_get_int(yyjson_obj_get(resources, "memory_gb"));
    if (memory_in_gbs == 0) {
        LOG_ERROR("Failed to read the 'memory_gb' field of the 'resources' section in the JSON!");
        return NULL;
    }

    res->ocpus = ocpus;
    res->memory_gb = memory_in_gbs;
    return res;
}

static Behaviour* construct_behaviour(yyjson_val *behaviour) {
    if (behaviour == NULL) {
        LOG_ERROR("Failed to read the 'behavior' section of the JSON body!");
        return NULL;
    }

    Behaviour *behav = malloc(sizeof(Behaviour));
    const int poll_interval_secs = yyjson_get_int(yyjson_obj_get(behaviour, "poll_interval_secs"));
    if (poll_interval_secs == 0) {
        LOG_ERROR("Failed to read the 'poll_interval_secs' field of the 'behavior' section in the JSON!");
        free(behav);
        return NULL;
    }

    const double ad_interval_secs = yyjson_get_real(yyjson_obj_get(behaviour, "ad_interval_secs"));
    if (ad_interval_secs <= 0.00001) {
        LOG_ERROR("Failed to read the 'ad_interval_secs' field of the 'behavior' section in the JSON!");
        free(behav);
        return NULL;
    }

    behav->ad_interval_secs = ad_interval_secs;
    behav->poll_interval_secs = poll_interval_secs;
    return behav;
}

ResourceConfig* load_resources_from_config(const char *path) {
    ResourceConfig *resources = calloc(1, sizeof(ResourceConfig));

    char pth_buf[256];
    getcwd(pth_buf, sizeof(pth_buf));
    
    char pth_with_file[strlen(pth_buf) + sizeof("/config.json")];
    sprintf(pth_with_file, "%s/config.json", pth_buf);
    
    char *json_string = read_to_string(path ? path : pth_with_file);
    if (json_string == NULL) {
        free_resource_config(resources);
        free(resources);
        return NULL;
    }

    // Read JSON and get root
    yyjson_doc *doc = yyjson_read(json_string, strlen(json_string), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *cfg = yyjson_obj_get(root, "config"); 
    yyjson_val *res = yyjson_obj_get(root, "resources");
    yyjson_val *behav = yyjson_obj_get(root, "behavior");
    
    Config* config_full = construct_config(cfg);
    if (config_full == NULL) {
        free_resource_config(resources);
        free(resources);

        free(json_string);
        yyjson_doc_free(doc);
        
        return NULL;
    }

    Resources *constructed_res = construct_resources(res);
    if (constructed_res == NULL) {
        free_resource_config(resources);
        free(resources);

        free(config_full);
        free(json_string);
        yyjson_doc_free(doc);
        
        return NULL;
    }

    Behaviour *behav_full = construct_behaviour(behav);
    if (behav_full == NULL) {
        free(config_full);  // already allocated!
        free(constructed_res);  // already allocated!
        free(json_string);
        yyjson_doc_free(doc);
        
        return NULL;
    }

    resources->config = config_full;
    resources->resources = constructed_res;
    resources->behaviour = behav_full;
    free(json_string);
    yyjson_doc_free(doc);

    return resources;
}

void free_resource_config(ResourceConfig *resconf) {
    if (resconf->resources) { 
        free_resources(resconf->resources); 
        free(resconf->resources); 
    }

    if (resconf->config) { 
        free_config(resconf->config); 
        free(resconf->config); 
    }

    free(resconf->behaviour);
}

void free_resources(Resources *res) {
    free(res->name);
    free(res->shape);
    res->memory_gb = 0;
    res->ocpus = 0;
}

void free_config(Config *cfg) {
    free(cfg->region);
    for (int i = 0; i < 3; i++) {
        free((void *)cfg->availability_domains[i]);
    }
}