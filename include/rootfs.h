#ifndef ROOTFS_H
#define ROOTFS_H

#define _GNU_SOURCE

int rootfs_mount(const char *rootfs_path);
int rootfs_pivot(const char *rootfs_path);
int rootfs_mount_defaults(void);
int rootfs_setup_files(const char *hostname);
int rootfs_setup_overlay(const char *image_path, const char *container_id);
#endif
