#ifndef NAMESPACE_H
#define NAMESPACE_H

#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>

typedef struct {
    int mount;
    int uts;
    int pid;
    int ipc;
    int network;
    int user;  // rootless için
    char hostname[64];
} namespace_config_t;

int namespace_setup(namespace_config_t *cfg);
int namespace_set_hostname(const char *hostname);

#endif
