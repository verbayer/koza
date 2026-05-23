#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "../include/config.h"
#include "../include/utils.h"

int config_load(const char *path, container_config_t *config) {
    if (!path || !config) {
        fprintf(stderr, "config_load: NULL parametre\n");
        return -1;
    }

    struct json_object *root = json_object_from_file(path);
    if (!root) {
        fprintf(stderr, "config_load: dosya okunamadi: %s\n", path);
        return -1;
    }

    struct json_object *val;
    struct json_object *cgroup;

    if (json_object_object_get_ex(root, "name", &val))
        strncpy(config->name, json_object_get_string(val), sizeof(config->name) - 1);
    if (json_object_object_get_ex(root, "rootfs", &val))
        strncpy(config->rootfs, json_object_get_string(val), sizeof(config->rootfs) - 1);
    if (json_object_object_get_ex(root, "command", &val))
        strncpy(config->command, json_object_get_string(val), sizeof(config->command) - 1);
    if (json_object_object_get_ex(root, "hostname", &val))
        strncpy(config->hostname, json_object_get_string(val), sizeof(config->hostname) - 1);
    if (json_object_object_get_ex(root, "capabilities", &val))
        strncpy(config->capabilities, json_object_get_string(val), sizeof(config->capabilities) - 1);
    if (json_object_object_get_ex(root, "rootless", &val))
        config->rootless = json_object_get_int(val);
    if (json_object_object_get_ex(root, "uid", &val))
        config->uid = (uid_t)json_object_get_int(val);
    if (json_object_object_get_ex(root, "gid", &val))
        config->gid = (gid_t)json_object_get_int(val);

    if (json_object_object_get_ex(root, "cgroup", &cgroup)) {
        if (json_object_object_get_ex(cgroup, "memory_limit", &val))
            config->cgroup.memory_limit = json_object_get_int64(val);
        if (json_object_object_get_ex(cgroup, "cpu_quota", &val))
            config->cgroup.cpu_quota = json_object_get_int64(val);
        if (json_object_object_get_ex(cgroup, "cpu_period", &val))
            config->cgroup.cpu_period = json_object_get_int64(val);
        if (json_object_object_get_ex(cgroup, "pids_limit", &val))
            config->cgroup.pids_limit = json_object_get_int64(val);
    }

    json_object_put(root);
    return 0;
}

int config_init(const char *name, const char *path) {
    if (!name || !path) {
        fprintf(stderr, "config_init: NULL parametre\n");
        return -1;
    }

    struct json_object *root   = json_object_new_object();
    struct json_object *cgroup = json_object_new_object();

    json_object_object_add(root, "name",         json_object_new_string(name));
    json_object_object_add(root, "rootfs",       json_object_new_string("/path/to/rootfs"));
    json_object_object_add(root, "command",      json_object_new_string("/bin/sh"));
    json_object_object_add(root, "hostname",     json_object_new_string(name));
    json_object_object_add(root, "rootless",     json_object_new_int(0));
    json_object_object_add(root, "capabilities", json_object_new_string(""));
    json_object_object_add(root, "uid", json_object_new_int(0));
    json_object_object_add(root, "gid", json_object_new_int(0));
    json_object_object_add(cgroup, "memory_limit", json_object_new_int64(0));
    json_object_object_add(cgroup, "cpu_quota",    json_object_new_int64(0));
    json_object_object_add(cgroup, "cpu_period",   json_object_new_int64(100000));
    json_object_object_add(cgroup, "pids_limit",   json_object_new_int64(0));
    json_object_object_add(root, "cgroup", cgroup);

    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    int ret = write_file(path, json_str);

    json_object_put(root);

    if (ret != 0) {
        fprintf(stderr, "config_init: dosya yazilamadi: %s\n", path);
        return -1;
    }

    printf("Config olusturuldu: %s\n", path);
    return 0;
}

int config_save(const char *path, container_config_t *config) {
    if (!path || !config) {
        fprintf(stderr, "config_save: NULL parametre\n");
        return -1;
    }

    struct json_object *root   = json_object_new_object();
    struct json_object *cgroup = json_object_new_object();

    json_object_object_add(root, "name",         json_object_new_string(config->name));
    json_object_object_add(root, "rootfs",       json_object_new_string(config->rootfs));
    json_object_object_add(root, "command",      json_object_new_string(config->command));
    json_object_object_add(root, "hostname",     json_object_new_string(config->hostname));
    json_object_object_add(root, "rootless",     json_object_new_int(config->rootless));
    json_object_object_add(root, "capabilities", json_object_new_string(config->capabilities));
    json_object_object_add(root, "uid",          json_object_new_int(config->uid));
    json_object_object_add(root, "gid",          json_object_new_int(config->gid));

    json_object_object_add(cgroup, "memory_limit", json_object_new_int64(config->cgroup.memory_limit));
    json_object_object_add(cgroup, "cpu_quota",    json_object_new_int64(config->cgroup.cpu_quota));
    json_object_object_add(cgroup, "cpu_period",   json_object_new_int64(config->cgroup.cpu_period));
    json_object_object_add(cgroup, "pids_limit",   json_object_new_int64(config->cgroup.pids_limit));
    json_object_object_add(root, "cgroup", cgroup);

    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    int ret = write_file(path, json_str);

    json_object_put(root);

    if (ret != 0) {
        fprintf(stderr, "config_save: yazılamadi: %s\n", path);
        return -1;
    }

    return 0;
}
