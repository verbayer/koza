#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <json-c/json.h>
#include "../include/state.h"
#include "../include/utils.h"

static const char *status_to_str(container_status_t status) {
    switch (status) {
        case CONTAINER_CREATED: return "created";
        case CONTAINER_RUNNING: return "running";
        case CONTAINER_STOPPED: return "stopped";
        default:                return "unknown";
    }
}

static container_status_t str_to_status(const char *str) {
    if (strcmp(str, "created") == 0) return CONTAINER_CREATED;
    if (strcmp(str, "running") == 0) return CONTAINER_RUNNING;
    if (strcmp(str, "stopped") == 0) return CONTAINER_STOPPED;
    return CONTAINER_STOPPED;
}

int state_save(container_state_t *state) {
    if (!state) {
        fprintf(stderr, "state_save: state NULL\n");
        return -1;
    }

    // Dizin oluştur: /var/lib/koza/containers/<id>
    char dir_path[PATH_MAX];
    if (path_join(dir_path, sizeof(dir_path), STATE_BASE_PATH, state->id) != 0)
        return -1;

    if (mkdir(dir_path, 0755) == -1 && errno != EEXIST) {
        perror("state_save: mkdir");
        return -1;
    }

    // JSON objesi oluştur
    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "id",      json_object_new_string(state->id));
    json_object_object_add(root, "name",    json_object_new_string(state->name));
    json_object_object_add(root, "rootfs",  json_object_new_string(state->rootfs));
    json_object_object_add(root, "command", json_object_new_string(state->command));
    json_object_object_add(root, "pid",     json_object_new_int(state->pid));
    json_object_object_add(root, "status",  json_object_new_string(status_to_str(state->status)));
    json_object_object_add(root, "veth_host", json_object_new_string(state->veth_host));
    json_object_object_add(root, "ip",        json_object_new_string(state->ip));
    json_object_object_add(root, "capabilities", json_object_new_string(state->capabilities));
    json_object_object_add(root, "uid",          json_object_new_int(state->uid));
    json_object_object_add(root, "gid",          json_object_new_int(state->gid));
    json_object_object_add(root, "rootless",     json_object_new_int(state->rootless));


    // state.json path'i
    char state_path[PATH_MAX];
    if (path_join(state_path, sizeof(state_path), dir_path, "state.json") != 0) {
        json_object_put(root);
        return -1;
    }

    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    int ret = write_file(state_path, json_str);

    json_object_put(root);
    return ret;
}

int state_load(const char *id, container_state_t *state) {
    if (!id || !state) {
        fprintf(stderr, "state_load: NULL parametre\n");
        return -1;
    }

    char dir_path[PATH_MAX];
    char state_path[PATH_MAX];
    if (path_join(dir_path, sizeof(dir_path), STATE_BASE_PATH, id) != 0)
        return -1;
    if (path_join(state_path, sizeof(state_path), dir_path, "state.json") != 0)
        return -1;

    struct json_object *root = json_object_from_file(state_path);
    if (!root) {
        fprintf(stderr, "state_load: dosya okunamadi: %s\n", state_path);
        return -1;
    }

    struct json_object *val;

    if (json_object_object_get_ex(root, "id", &val))
        strncpy(state->id, json_object_get_string(val), sizeof(state->id) - 1);
    
    if (json_object_object_get_ex(root, "name", &val))
        strncpy(state->name, json_object_get_string(val), sizeof(state->name) - 1);
    
    if (json_object_object_get_ex(root, "rootfs", &val))
        strncpy(state->rootfs, json_object_get_string(val), sizeof(state->rootfs) - 1);
    
    if (json_object_object_get_ex(root, "command", &val))
        strncpy(state->command, json_object_get_string(val), sizeof(state->command) - 1);
    
    if (json_object_object_get_ex(root, "pid", &val))
        state->pid = (pid_t)json_object_get_int(val);
    
    if (json_object_object_get_ex(root, "status", &val))
        state->status = str_to_status(json_object_get_string(val));

    if (json_object_object_get_ex(root, "veth_host", &val))
    strncpy(state->veth_host, json_object_get_string(val), sizeof(state->veth_host) - 1);
    
    if (json_object_object_get_ex(root, "ip", &val))
    strncpy(state->ip, json_object_get_string(val), sizeof(state->ip) - 1);
    
    if (json_object_object_get_ex(root, "capabilities", &val))
    strncpy(state->capabilities, json_object_get_string(val), sizeof(state->capabilities) - 1);

    if (json_object_object_get_ex(root, "uid", &val))
    state->uid = (uid_t)json_object_get_int(val);

    if (json_object_object_get_ex(root, "gid", &val))
    state->gid = (gid_t)json_object_get_int(val);

    if (json_object_object_get_ex(root, "rootless", &val))
    state->rootless = json_object_get_int(val);

    json_object_put(root);
    return 0;
}

int state_delete(const char *id) {
    if (!id) {
        fprintf(stderr, "state_delete: id NULL\n");
        return -1;
    }

    char dir_path[PATH_MAX];
    char state_path[PATH_MAX];
    if (path_join(dir_path, sizeof(dir_path), STATE_BASE_PATH, id) != 0)
        return -1;
    if (path_join(state_path, sizeof(state_path), dir_path, "state.json") != 0)
        return -1;

    if (remove(state_path) == -1) {
        perror("state_delete: remove");
        return -1;
    }
    if (rm_r(dir_path) != 0) {
    fprintf(stderr, "state_delete: dizin silinemedi\n");
    return -1;
    }
    
    return 0;
}

int state_list(container_state_t *states, int max_count) {
    if (!states || max_count <= 0) {
        fprintf(stderr, "state_list: gecersiz parametre\n");
        return -1;
    }

    DIR *dir = opendir(STATE_BASE_PATH);
    if (!dir) {
        // Hiç container yoksa dizin olmayabilir, hata değil
        if (errno == ENOENT) return 0;
        perror("state_list: opendir");
        return -1;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        // . ve .. atla, sadece dizinleri al
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type != DT_DIR)  continue;

        if (state_load(entry->d_name, &states[count]) == 0)
            count++;
    }

    closedir(dir);
    return count;  // yüklenen container sayısını döndür
}
