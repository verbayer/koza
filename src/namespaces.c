#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/utsname.h>
#include "../include/namespaces.h"
#include <signal.h>

static int build_clone_flags(namespace_config_t *cfg) {
    int flags = SIGCHLD;

    if (cfg->mount)   flags |= CLONE_NEWNS;
    if (cfg->uts)     flags |= CLONE_NEWUTS;
    if (cfg->pid)     flags |= CLONE_NEWPID;
    if (cfg->ipc)     flags |= CLONE_NEWIPC;
    if (cfg->network) flags |= CLONE_NEWNET;
    if (cfg->user)    flags |= CLONE_NEWUSER;

    return flags;
}

int namespace_setup(namespace_config_t *cfg) {
    if (!cfg) {
        fprintf(stderr, "namespace_setup: cfg NULL\n");
        return -1;
    }

    int flags = build_clone_flags(cfg);

    if (unshare(flags & ~SIGCHLD) == -1) {
        perror("namespace_setup: unshare");
	fprintf(stderr, "flags degeri: %d\n", flags & ~SIGCHLD);
        return -1;
    }

    if (cfg->uts && strlen(cfg->hostname) > 0) {
        if (namespace_set_hostname(cfg->hostname) != 0)
            return -1;
    }

    return 0;
}

int namespace_set_hostname(const char *hostname) {
    if (sethostname(hostname, strlen(hostname)) == -1) {
        perror("namespace_set_hostname: sethostname");
        return -1;
    }
    return 0;
}
