#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include "../include/rootfs.h"
#include "../include/utils.h"

int rootfs_mount(const char *rootfs_path){

    if (mount(rootfs_path, rootfs_path, NULL, MS_BIND | MS_REC, NULL) == -1) {
        perror("rootfs_mount: bind mount");
        return -1;
    }
    return 0;

}

int rootfs_setup_overlay(const char *image_path, const char *container_id) {
    char upper[PATH_MAX+7];
    char work[PATH_MAX+6];
    char merged[PATH_MAX+8];
    char opts[PATH_MAX * 3 + 64];  // 3 path + prefix'ler için
    char base[PATH_MAX];

    // Dizin yollarını oluştur
    snprintf(base,   sizeof(base),   "/var/lib/koza/containers/%s", container_id);
    snprintf(upper,  sizeof(upper),  "%s/upper",  base);
    snprintf(work,   sizeof(work),   "%s/work",   base);
    snprintf(merged, sizeof(merged), "%s/merged", base);

    // Dizinleri oluştur
    if (mkdir_p(upper,  0755) != 0) return -1;
    if (mkdir_p(work,   0755) != 0) return -1;
    if (mkdir_p(merged, 0755) != 0) return -1;

    // Overlay mount seçenekleri
    snprintf(opts, sizeof(opts),
        "lowerdir=%s,upperdir=%s,workdir=%s",
        image_path, upper, work);

    if (mount("overlay", merged, "overlay", 0, opts) == -1) {
        if(errno==EBUSY) return 0;
	perror("rootfs_setup_overlay: mount");
        return -1;
    }

    return 0;
}

int rootfs_pivot(const char *rootfs_path){

    // Mevcut root'u private yap, pivot_root bunu gerektirir
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("rootfs_pivot: make private");
        return -1;
    }

    char put_old[PATH_MAX];

    if (path_join(put_old, sizeof(put_old), rootfs_path, ".pivot_old") != 0)
        return -1;
    	

    if (mkdir(put_old, 0700) == -1 && errno != EEXIST) {
        perror("rootfs_pivot: mkdir put_old");
        return -1;
    }

    if (syscall(SYS_pivot_root, rootfs_path, put_old) == -1) {
        perror("rootfs_pivot: pivot_root");
        return -1;
    }

    if (chdir("/") == -1) {
        perror("rootfs_pivot: chdir");
        return -1;
    }

    if (umount2("/.pivot_old", MNT_DETACH) == -1) {
        perror("rootfs_pivot: umount2");
        return -1;
    }

    if (rmdir("/.pivot_old") == -1) {
        perror("rootfs_pivot: rmdir");
        return -1;
    }

    return 0;	
}

int rootfs_mount_defaults(void){

    // proc: process bilgileri icin
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == -1) {
        perror("rootfs_mount_defaults: /proc");
        return -1;
    }

    // sysfs: kernel/device bilgileri icin, readonly
    if (mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY, NULL) == -1) {
        perror("rootfs_mount_defaults: /sys");
        return -1;
    }

    // tmpfs: /dev icin
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755") == -1) {
        perror("rootfs_mount_defaults: /dev");
        return -1;
    }

    // devpts: terminal desteği icin
    if (mkdir("/dev/pts", 0755) == -1 && errno != EEXIST) {
        perror("rootfs_mount_defaults: mkdir /dev/pts");
        return -1;
    }
    if (mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "newinstance,ptmxmode=0666") == -1) {
        perror("rootfs_mount_defaults: /dev/pts");
        return -1;
    }

    // tmpfs: /tmp icin
    if (mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") == -1) {
        perror("rootfs_mount_defaults: /tmp");
        return -1;
    }

    return 0;
}

int rootfs_setup_files(const char *hostname){

     // /etc/hostname yaz
    if (write_file("/etc/hostname", hostname) != 0) {
        fprintf(stderr, "rootfs_setup_files: hostname yazilamadi\n");
        return -1;
    }

    // /etc/resolv.conf yaz - Google DNS
    if (write_file("/etc/resolv.conf", "nameserver 8.8.8.8\nnameserver 8.8.4.4\n") != 0) {
        fprintf(stderr, "rootfs_setup_files: resolv.conf yazilamadi\n");
        return -1;
    }

    return 0;

}

