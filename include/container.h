#ifndef CONTAINER_H
#define CONTAINER_H

#include <sys/types.h>
#include "state.h"

typedef struct {
    char name[64];
    char rootfs[256];
    char command[256];
    char capabilities[256];
    char hostname[64];
    int rootless;
    uid_t uid;  // default 0
    gid_t gid;  // default 0
    int interactive;
    struct {
        long memory_limit;
        long cpu_quota;
        long cpu_period;
        long pids_limit;
    } cgroup;
} container_config_t;

int container_create(container_config_t *config, char *id_out, size_t id_out_size);
int container_run(const char *id,int interactive);
int container_stop(const char *id);
int container_delete(const char *id);
int container_list(void);

#endif
