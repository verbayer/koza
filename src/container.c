#define _GNU_SOURCE
#include <stdio.h>
#include <pty.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/mount.h>
#include "../include/container.h"
#include "../include/cgroups.h"
#include "../include/namespaces.h"
#include "../include/rootfs.h"
#include "../include/caps.h"
#include "../include/state.h"
#include "../include/utils.h"
#include "../include/network.h"
#include "../include/pty.h"
#include "../include/config.h"
#define STACK_SIZE (1024 * 1024)

// Container child process'inin ihtiyaç duyduğu argümanlar
typedef struct {
    container_config_t *config;
    char id[17];
    char merged[PATH_MAX];
    int pipe_read_fd;
    int pipe_write_fd;
    int slave_fd;
    int interactive;
} child_args_t;

static int container_child(void *arg) {
    child_args_t *args = (child_args_t *)arg;
    container_config_t *config = args->config;

    if (config->rootless) {
        char buf[1];
        close(args->pipe_write_fd);  
        read(args->pipe_read_fd, buf, 1);
    }

    // 1. Hostname set et
    if (namespace_set_hostname(config->hostname) != 0){
	    return -1;}

    // 2. Rootfs hazırla
    if (rootfs_pivot(args->merged) != 0){ return -1;
    }
    
    if (rootfs_mount_defaults() != 0){
	    
	    return -1;}


    if (rootfs_setup_files(config->hostname) != 0){
	    return -1;}


    // 3. KULLANICI AYARLARI (Capability'lerden ÖNCE yapılmalı)
    if (config->uid != 0 || config->gid != 0) {
        // Eğer root kullanıcısından normal kullanıcıya geçiş varsa,
        // kernel'a mevcut capability'leri korumasını söylüyoruz (KEEPCAPS)
        if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) == -1) {
            perror("container_child: prctl(PR_SET_KEEPCAPS)");
            return -1;
        }

        if (setgid(config->gid) == -1) {
            perror("container_child: setgid");
            return -1;
        }
        if (setuid(config->uid) == -1) {
            perror("container_child: setuid");
            return -1;
        }
    }

    // 4. CAPABILITY AYARLARI
    if (strlen(config->capabilities) > 0) {
        // String'i cap_t yapısına dönüştür
        cap_t allowed = cap_from_text(config->capabilities);
        if (!allowed) {
            perror("container_child: cap_from_text");
            return -1;
        }
        // setuid/setgid sonrası effective kümesi temizlenmiş olabileceği için
        // process'e hedef capability maskesini tekrar giydiriyoruz
        if (cap_set_proc(allowed) == -1) {
            perror("container_child: cap_set_proc");
            cap_free(allowed);
            return -1;
        }


        // execve sonrasına aktarmak için Ambient setini yükselt
        if (caps_set_ambient_from_string(config->capabilities) != 0) {
            cap_free(allowed);
            return -1;

    	    // Bounding setten sadece listede OLMAYANLARI temizle
        if (caps_drop_bounding(allowed) != 0) {
            cap_free(allowed);
            return -1;
        }
        }

        cap_free(allowed);
    } else {
        // Eğer hiçbir yetki istenmediyse her şeyi temizle
        if (caps_drop_bounding(NULL) != 0)
            return -1;
        if (caps_drop_all() != 0)
            return -1;
    }

    if (args->interactive) {
        if (pty_setup_child(args->slave_fd) != 0)
            return -1;
    }

    // 5. Komutu çalıştır
    char *argv[] = {"/bin/sh", "-c", config->command, NULL };
    
    char *envp[] = {
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "HOME=/root",
        "TERM=xterm",
        NULL
    };

    execve("/bin/sh", argv, envp);

    perror("container_child: execve");
    return -1;
}

int container_create(container_config_t *config, char *id_out, size_t id_out_size) {
    if (!config) {
        fprintf(stderr, "container_create: config NULL\n");
        return -1;
    }

    // ID üret
    container_state_t state;
    memset(&state, 0, sizeof(state));
    if (generate_id(state.id, sizeof(state.id)) != 0)
        return -1;

    // State doldur
    strncpy(state.name,    config->name,    sizeof(state.name) - 1);
    strncpy(state.rootfs,  config->rootfs,  sizeof(state.rootfs) - 1);
    strncpy(state.command, config->command, sizeof(state.command) - 1);
    strncpy(state.capabilities, config->capabilities, sizeof(state.capabilities) - 1);
    state.uid = config->uid;
    state.gid = config->gid;
    state.rootless = config->rootless;
    state.pid    = 0;
    state.status = CONTAINER_CREATED;
    // Cgroup oluştur
    if (cgroup_init(state.id) != 0)
        return -1;

    // Cgroup limitleri set et
    if (config->cgroup.memory_limit > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", config->cgroup.memory_limit);
        cgroup_set_limit(state.id, "memory.max", buf);
    }
    if (config->cgroup.cpu_quota > 0 && config->cgroup.cpu_period > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld %ld",
            config->cgroup.cpu_quota,
            config->cgroup.cpu_period);
        cgroup_set_limit(state.id, "cpu.max", buf);
    }
    if (config->cgroup.pids_limit > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", config->cgroup.pids_limit);
        cgroup_set_limit(state.id, "pids.max", buf);
    }

    // State kaydet
    if (state_save(&state) != 0) {
        cgroup_cleanup(state.id);
        return -1;
    }

    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path),
         "/var/lib/koza/containers/%s/config.json", state.id);
    config_save(config_path, config);

    printf("Container olusturuldu: %s (%s)\n", state.name, state.id);
    strncpy(id_out, state.id, id_out_size - 1);
    return 0;
}


int container_run(const char *id,int interactive) {
    if (!id) {
        fprintf(stderr, "container_run: id NULL\n");
        return -1;
    }

    // State yükle
    container_state_t state;
    if (state_load(id, &state) != 0)
        return -1;
    if (state.status == CONTAINER_RUNNING) {
        fprintf(stderr, "container_run: container zaten calisiyor\n");
        return -1;
    }

    // Config'i state'den doldur
    container_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.name,     state.name,    sizeof(config.name) - 1);
    strncpy(config.rootfs,   state.rootfs,  sizeof(config.rootfs) - 1);
    strncpy(config.command,  state.command, sizeof(config.command) - 1);
    strncpy(config.hostname, state.name,    sizeof(config.hostname) - 1);
    strncpy(config.capabilities, state.capabilities, sizeof(config.capabilities) - 1);
    config.uid = state.uid;
    config.gid = state.gid;
    config.rootless = state.rootless;
    config.interactive = interactive;
    
    // Config dosyasından limitleri oku
    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path),
         "/var/lib/koza/containers/%s/config.json", id);

    container_config_t full_config;
    memset(&full_config, 0, sizeof(full_config));
    config_load(config_path, &full_config);

    // Cgroup yeniden oluştur
    if (cgroup_init(id) != 0)
        return -1;

    // Limitleri yeniden uygula
    if (full_config.cgroup.memory_limit > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", full_config.cgroup.memory_limit);
        cgroup_set_limit(id, "memory.max", buf);
    }

    if (full_config.cgroup.cpu_quota > 0 && full_config.cgroup.cpu_period > 0) {
         char buf[32];
         snprintf(buf, sizeof(buf), "%ld %ld", full_config.cgroup.cpu_quota, full_config.cgroup.cpu_period);
          cgroup_set_limit(id, "cpu.max", buf);
    }

    if (full_config.cgroup.pids_limit > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", full_config.cgroup.pids_limit);
        cgroup_set_limit(id, "pids.max", buf);
    }
    
    // Stack ayır
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("container_run: malloc");
        return -1;
    }

    // Namespace flagleri
    int flags = CLONE_NEWPID  |
                CLONE_NEWNS   |
                CLONE_NEWNET  |
                CLONE_NEWIPC  |
                CLONE_NEWUTS  |
                SIGCHLD;

    if (config.rootless)
        flags |= CLONE_NEWUSER;

    // Args hazırla
    child_args_t args;
    memset(&args, 0, sizeof(args));
    args.config = &config;
    strncpy(args.id, id, sizeof(args.id) - 1);

    // Rootless ise pipe oluştur
    int pipefd[2];
    if (config.rootless) {
        if (pipe(pipefd) == -1) {
            perror("container_run: pipe");
            free(stack);
            return -1;
        }
        args.pipe_read_fd  = pipefd[0];
        args.pipe_write_fd = pipefd[1];
    }

    // Overlay kur
    if (rootfs_setup_overlay(state.rootfs, id) != 0)
        return -1;

    // Merged path'i args'a ekle
    snprintf(args.merged, sizeof(args.merged),"/var/lib/koza/containers/%s/merged", id);
   
    int master_fd = -1;

    if (config.interactive) {
        int slave_fd;
        if (openpty(&master_fd, &slave_fd, NULL, NULL, NULL) == -1) {
            perror("container_run: openpty");
            free(stack);
            return -1;
        }
        args.slave_fd   = slave_fd;
        args.interactive = 1;
    }

    // Container process'ini başlat
    pid_t pid = clone(container_child,
                      stack + STACK_SIZE,
                      flags,
                      &args);
    free(stack);

    if (pid == -1) {
        perror("container_run: clone");
    	fprintf(stderr, "errno: %d\n", errno);
    	if (config.rootless) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return -1;
    }
    // Rootless ise uid/gid mapping yaz, sonra child'ı serbest bırak
    if (config.rootless) {
        char path[64];
        char buf[64];

        snprintf(path, sizeof(path), "/proc/%d/uid_map", pid);
        snprintf(buf,  sizeof(buf),  "0 %d 1", getuid());
        write_file(path, buf);

        snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
        write_file(path, "deny");

        snprintf(path, sizeof(path), "/proc/%d/gid_map", pid);
        snprintf(buf,  sizeof(buf),  "0 %d 1", getgid());
        write_file(path, buf);

        close(pipefd[0]);  // okuma ucunu kapat
        close(pipefd[1]);  // yazma ucunu kapat, child devam etsin
    }
    // Network kur
    network_config_t net_cfg;
    memset(&net_cfg, 0, sizeof(net_cfg));
    snprintf(net_cfg.veth_host, sizeof(net_cfg.veth_host), "veth%.8s", id);
    strncpy(net_cfg.veth_cont, "eth0", sizeof(net_cfg.veth_cont) - 1);
    snprintf(net_cfg.ip, sizeof(net_cfg.ip), "10.0.0.%d", (pid % 253) + 2);
    strncpy(net_cfg.gateway, KOZA_GATEWAY, sizeof(net_cfg.gateway) - 1);

    if (network_setup_container(pid, &net_cfg) != 0) {
        fprintf(stderr, "container_run: network kurulamadi\n");
        return -1;
    }   

    // State'e network bilgilerini kaydet
    strncpy(state.veth_host, net_cfg.veth_host, sizeof(state.veth_host) - 1);
    strncpy(state.ip, net_cfg.ip, sizeof(state.ip) - 1);
    // Cgroup'a ekle
    if (cgroup_add_process(id, pid) != 0) {
	return -1;
    }
    // State güncelle
    state.pid    = pid;
    state.status = CONTAINER_RUNNING;
    if (state_save(&state) != 0) {
    	return -1;
    }
    printf("Container baslatildi: %s (pid: %d)\n", state.name, pid);
    if (config.interactive) {
    close(args.slave_fd);  // parent slave'i kapatır
    pty_run(master_fd);    // container bitene kadar burada bekler

    // Overlay unmount et
    char merged[PATH_MAX];
    snprintf(merged, sizeof(merged), "/var/lib/koza/containers/%s/merged", id);
    umount2(merged, MNT_DETACH);

    close(master_fd);
    /// Shell çıktı, state'i güncelle
    state.status = CONTAINER_STOPPED;
    state.pid    = 0;
    state_save(&state);
    // Cgroup temizle
    cgroup_cleanup(id);
    }
    
    
    return 0;
}


int container_stop(const char *id) {
    if (!id) {
        fprintf(stderr, "container_stop: id NULL\n");
        return -1;
    }

    container_state_t state;
    if (state_load(id, &state) != 0)
        return -1;

    if (state.status != CONTAINER_RUNNING) {
        fprintf(stderr, "container_stop: container calismiyor\n");
        return -1;
    }

    if (!is_process_alive(state.pid)) {
        fprintf(stderr, "container_stop: process zaten olmus\n");
        state.status = CONTAINER_STOPPED;
        state_save(&state);
        return 0;
    }

    // Önce SIGTERM
    if (kill(state.pid, SIGTERM) == -1) {
        perror("container_stop: SIGTERM");
        return -1;
    }

    // 5 saniye bekle, hala çalışıyorsa zorla kapat
    sleep(5);
    if (is_process_alive(state.pid)) {
        if (kill(state.pid, SIGKILL) == -1) {
            perror("container_stop: SIGKILL");
            return -1;
        }
    }

    waitpid(state.pid, NULL, 0);

    char merged[PATH_MAX];
    snprintf(merged, sizeof(merged), "/var/lib/koza/containers/%s/merged", id);
    if (umount2(merged, MNT_DETACH) == -1 && errno != EINVAL && errno != ENOENT) {
        perror("container_stop: overlay unmount");
    }
    
    state.status = CONTAINER_STOPPED;
    state.pid    = 0;
    state_save(&state);

    printf("Container durduruldu: %s\n", state.name);
    return 0;
}

int container_delete(const char *id) {
    if (!id) {
        fprintf(stderr, "container_delete: id NULL\n");
        return -1;
    }

    container_state_t state;
    if (state_load(id, &state) != 0)
        return -1;

    if (state.status == CONTAINER_RUNNING) {
        fprintf(stderr, "container_delete: once durdurun (koza stop %s)\n", id);
        return -1;
    }

    // Cgroup temizle
    cgroup_cleanup(id);
    
    //network temizle
    network_cleanup(state.veth_host);
    // State sil
    if (state_delete(id) != 0)
        return -1;

    printf("Container silindi: %s\n", state.name);
    return 0;
}

int container_list(void) {
    container_state_t states[128];
    int count = state_list(states, 128);

    if (count == 0) {
        printf("Hic container yok.\n");
        return 0;
    }

    printf("%-16s %-20s %-10s %-6s\n", "ID", "NAME", "STATUS", "PID");
    printf("%-16s %-20s %-10s %-6s\n", "---", "----", "------", "---");

    for (int i = 0; i < count; i++) {
        const char *status_str;
        switch (states[i].status) {
            case CONTAINER_CREATED: status_str = "created"; break;
            case CONTAINER_RUNNING: status_str = "running"; break;
            case CONTAINER_STOPPED: status_str = "stopped"; break;
            default:                status_str = "unknown"; break;
        }

        printf("%-16s %-20s %-10s %-6d\n",
            states[i].id,
            states[i].name,
            status_str,
            states[i].pid);
    }

    return 0;
}
