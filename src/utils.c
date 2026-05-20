#define _GNU_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <ftw.h>
#include "../include/utils.h"

int write_file(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("write_file: fopen");
        return -1;
    }
    if (fprintf(f, "%s", value) < 0) {
        perror("write_file: fprintf");
        fclose(f);
        return -1;
    }
    if (fflush(f) != 0 || ferror(f)) {
        perror("write_file: flush");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int path_join(char *out, size_t out_size, const char *base, const char *sub) {
    int written = snprintf(out, out_size, "%s/%s", base, sub);
    if (written < 0 || (size_t)written >= out_size) {
        fprintf(stderr, "path_join: path cok uzun\n");
        return -1;
    }
    return 0;
}

int generate_id(char *out, size_t out_size) {
    unsigned char buf[8];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        perror("generate_id: /dev/urandom");
        return -1;
    }
    if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("generate_id: read");
        close(fd);
        return -1;
    }
    close(fd);

    int written = snprintf(out, out_size,
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5], buf[6], buf[7]);

    if (written < 0 || (size_t)written >= out_size) {
        fprintf(stderr, "generate_id: buffer cok kucuk\n");
        return -1;
    }
    return 0;
}

int is_process_alive(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    return (access(path, F_OK) == 0) ? 1 : 0;
}

int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0'; 
            
            if (mkdir(tmp, mode) == -1) {
                if (errno != EEXIST) {
                    return -1; 
                }
            }
            
            *p = '/'; // Slash'ı geri koy
            // Ardışık slashları (//) atla
            while (*(p + 1) == '/') p++;
        }
    }

    if (mkdir(tmp, mode) == -1 && errno != EEXIST) {
        return -1;
    }

    return 0;
}


static int rm_entry(const char *path, const struct stat *sb,
                    int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (typeflag == FTW_DP)
        return rmdir(path);
    else
        return remove(path);
}

int rm_r(const char *path) {
    return nftw(path, rm_entry, 64, FTW_DEPTH | FTW_PHYS);
}
