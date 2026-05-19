#ifndef STATE_H
#define STATE_H

#include <sys/types.h>

#define STATE_BASE_PATH "/var/lib/koza/containers"

typedef enum {
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED
} container_status_t;

typedef struct {
    char id[17];           // 16 hex + null terminator
    char name[64];
    char rootfs[256];
    char command[256];
    pid_t pid;
    container_status_t status;
    char veth_host[16];
    char ip[16];
    char capabilities[256];
    uid_t uid;
    gid_t gid;
    int rootless;
} container_state_t;

int state_save(container_state_t *state);
int state_load(const char *id, container_state_t *state);
int state_delete(const char *id);
int state_list(container_state_t *states, int max_count);

#endif
