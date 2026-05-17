#ifndef CGROUPS_H  // Header Guard: Dosyanın birden fazla kez eklenmesini önler
#define CGROUPS_H

#include <sys/types.h> 
int cgroup_init(const char *container_id);
int cgroup_set_limit(const char *container_id, const char *file, const char *value);
int cgroup_add_process(const char *container_id, pid_t pid);
int cgroup_cleanup(const char *container_id);

#endif
