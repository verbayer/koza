#define _GNU_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/mount.h>
#include <errno.h> 
#include "../include/cgroups.h"
#include "../include/utils.h"
#include <limits.h>
#define CG_BASE_PATH "/sys/fs/cgroup"

static int write_to_cgroup(const char *container_id, const char *file, const char *value) {
    char cgroup_path[PATH_MAX];
    char full_path[PATH_MAX];
    
    if (path_join(cgroup_path, sizeof(cgroup_path), CG_BASE_PATH, container_id) != 0)
        return -1;
    if (path_join(full_path, sizeof(full_path), cgroup_path, file) != 0)
        return -1;
    
    return write_file(full_path, value);
}

int cgroup_init(const char *container_id){
	char path[256];
	snprintf(path, sizeof(path), "%s/%s", CG_BASE_PATH, container_id);
	if(mkdir(path,0755)!=0 && errno != EEXIST){
	perror("mkdir olmadı.");return -1;}
	return 0;

}
int cgroup_set_limit(const char *container_id, const char *file, const char *value){
	return write_to_cgroup(container_id,file,value);
}

int cgroup_add_process(const char *container_id, pid_t pid){
	char pid_str[32];
	snprintf(pid_str, sizeof(pid_str), "%d\n", pid);
	return write_to_cgroup(container_id, "cgroup.procs", pid_str);
}
int cgroup_cleanup(const char *container_id){
	char path[256];
       	snprintf(path, sizeof(path), "%s/%s", CG_BASE_PATH, container_id);
    
	if (rmdir(path) == -1) {
        	if (errno == ENOENT) return 0;  // zaten silinmiş, sorun değil
                if (errno == EBUSY) {
                    fprintf(stderr, "Cgroup hala aktif process iceriyor: %s\n", path);
        	return -1;
     		}
		perror("Cgroup silinemedi");
        return -1;
    }

        printf("Cgroup basariyla silindi: %s\n", path);
	return 0;
}
