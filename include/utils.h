#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

int write_file(const char *path, const char *value);
int path_join(char *out, size_t out_size, const char *base, const char *sub);
int generate_id(char *out, size_t out_size); // id stringleri min 17 byte olacak
int is_process_alive(pid_t pid);
int mkdir_p(const char *path, mode_t mode);
int rm_r(const char *path);
#endif
