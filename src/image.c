#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <archive.h>
#include <archive_entry.h>
#include "../include/image.h"
#include "../include/state.h"
#include "../include/utils.h"
#include "../include/container.h"
#include "../include/config.h"

static int add_dir_to_archive(struct archive *a, const char *dir_path, const char *prefix) {
    struct archive *disk = archive_read_disk_new();
    archive_read_disk_set_standard_lookup(disk);

    int r = archive_read_disk_open(disk, dir_path);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "add_dir_to_archive: %s acilamadi: %s\n",
                dir_path, archive_error_string(disk));
        archive_read_free(disk);
        return -1;
    }

    struct archive_entry *entry;
    char archive_path[PATH_MAX];

    while (archive_read_next_header2(disk, entry = archive_entry_new()) == ARCHIVE_OK) {
        archive_read_disk_descend(disk);

        const char *entry_path = archive_entry_sourcepath(entry);

        // Relative path hesapla
        const char *rel = entry_path + strlen(dir_path);
        while (*rel == '/') rel++;

        if (prefix && strlen(prefix) > 0)
            snprintf(archive_path, sizeof(archive_path), "%s/%s", prefix, rel);
        else
            snprintf(archive_path, sizeof(archive_path), "%s", rel);

        archive_entry_set_pathname(entry, archive_path);

        r = archive_write_header(a, entry);
        if (r != ARCHIVE_OK) {
            fprintf(stderr, "add_dir_to_archive: header yazılamadi: %s\n",
                    archive_error_string(a));
            archive_entry_free(entry);
            continue;
        }

        // Düzenli dosyaysa içeriğini yaz
        if (archive_entry_size(entry) > 0) {
            FILE *f = fopen(entry_path, "rb");
            if (f) {
                char buf[8192];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
                    archive_write_data(a, buf, n);
                fclose(f);
            }
        }

        archive_entry_free(entry);
    }

    archive_read_free(disk);
    return 0;
}

int image_export(const char *container_id) {
    if (!container_id) {
        fprintf(stderr, "image_export: NULL parametre\n");
        return -1;
    }

    // State yükle
    container_state_t state;
    if (state_load(container_id, &state) != 0)
        return -1;

    if (state.status == CONTAINER_RUNNING) {
        fprintf(stderr, "image_export: container once durdurulmali\n");
        return -1;
    }

    // Dizin yolları
    char upper[PATH_MAX];
    char config_path[PATH_MAX];
    char output_path[PATH_MAX];

    snprintf(upper,       sizeof(upper),       "/var/lib/koza/containers/%s/upper",       container_id);
    snprintf(config_path, sizeof(config_path), "/var/lib/koza/containers/%s/config.json", container_id);
    snprintf(output_path, sizeof(output_path), "%s.koza", container_id);

    // Archive oluştur
    struct archive *a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);

    if (archive_write_open_filename(a, output_path) != ARCHIVE_OK) {
        fprintf(stderr, "image_export: dosya acilamadi: %s\n", archive_error_string(a));
        archive_write_free(a);
        return -1;
    }

    // 1. Base imajı ekle
    printf("Base imaj ekleniyor: %s\n", state.rootfs);
    if (add_dir_to_archive(a, state.rootfs, "") != 0) {
        archive_write_free(a);
        return -1;
    }

    // 2. Upper dizinini üzerine yaz
    printf("Container degisiklikleri ekleniyor: %s\n", upper);
    if (add_dir_to_archive(a, upper, "") != 0) {
        archive_write_free(a);
        return -1;
    }

    // 3. config.json'ı ekle
    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, "koza_config.json");
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);

    FILE *cfg_f = fopen(config_path, "r");
    if (cfg_f) {
        fseek(cfg_f, 0, SEEK_END);
        long cfg_size = ftell(cfg_f);
        fseek(cfg_f, 0, SEEK_SET);

        archive_entry_set_size(entry, cfg_size);
        archive_write_header(a, entry);

        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), cfg_f)) > 0)
            archive_write_data(a, buf, n);

        fclose(cfg_f);
    } else {
        fprintf(stderr, "image_export: config.json bulunamadi, atlanıyor\n");
        archive_entry_set_size(entry, 0);
        archive_write_header(a, entry);
    }

    archive_entry_free(entry);
    archive_write_close(a);
    archive_write_free(a);

    printf("Image olusturuldu: %s\n", output_path);
    return 0;
}

int image_import(const char *archive_path) {
    if (!archive_path) {
        fprintf(stderr, "image_import: NULL parametre\n");
        return -1;
    }

    // Geçici dizin oluştur, config'den ismi aldıktan sonra yeniden adlandırırız
    char tmp_dir[PATH_MAX];
    char tmp_rootfs[PATH_MAX+8];
    char config_path[PATH_MAX+18];

    snprintf(tmp_dir,    sizeof(tmp_dir),    "%s/_tmp_import",        IMAGE_BASE_PATH);
    snprintf(tmp_rootfs, sizeof(tmp_rootfs), "%s/rootfs",             tmp_dir);
    snprintf(config_path,sizeof(config_path),"%s/koza_config.json",   tmp_dir);

    if (mkdir_p(tmp_rootfs, 0755) != 0) {
        fprintf(stderr, "image_import: dizin olusturulamadi\n");
        return -1;
    }

    // Archive aç
    struct archive *a = archive_read_new();
    archive_read_support_filter_gzip(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, archive_path, 8192) != ARCHIVE_OK) {
        fprintf(stderr, "image_import: arsiv acilamadi: %s\n", archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    struct archive *disk = archive_write_disk_new();
    archive_write_disk_set_options(disk,
        ARCHIVE_EXTRACT_TIME |
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_ACL  |
        ARCHIVE_EXTRACT_FFLAGS);
    archive_write_disk_set_standard_lookup(disk);

    struct archive_entry *entry;
    char full_path[PATH_MAX + PATH_MAX + 8 + 2];

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *entry_name = archive_entry_pathname(entry);

        if (strcmp(entry_name, "koza_config.json") == 0) {
            archive_entry_set_pathname(entry, config_path);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", tmp_rootfs, entry_name);
            archive_entry_set_pathname(entry, full_path);
        }

        int r = archive_write_header(disk, entry);
        if (r != ARCHIVE_OK) {
            fprintf(stderr, "image_import: header yazılamadi: %s\n",
                    archive_error_string(disk));
            continue;
        }

        const void *buf;
        size_t size;
        la_int64_t offset;

        while (archive_read_data_block(a, &buf, &size, &offset) == ARCHIVE_OK)
            archive_write_data_block(disk, buf, size, offset);
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(disk);
    archive_write_free(disk);

    // Config'i oku
    container_config_t config;
    memset(&config, 0, sizeof(config));

    if (config_load(config_path, &config) != 0) {
        fprintf(stderr, "image_import: config yuklenemedi\n");
        rm_r(tmp_dir);
        return -1;
    }

    // İsmi config'den al, dizini yeniden adlandır
    char final_dir[PATH_MAX];
    snprintf(final_dir, sizeof(final_dir), "%s/%s", IMAGE_BASE_PATH, config.name);

    if (access(final_dir, F_OK) == 0) {
    fprintf(stderr, "image_import: '%s' isimli imaj zaten var, önce silin veya içeriğini kontrol edin.\n", config.name);
    rm_r(tmp_dir);
    return -1;
    }

    if (rename(tmp_dir, final_dir) == -1) {
        perror("image_import: rename");
        rm_r(tmp_dir);
        return -1;
    }

    // rootfs'i yeni dizine güncelle
    char final_rootfs[PATH_MAX+8];
    snprintf(final_rootfs, sizeof(final_rootfs), "%s/rootfs", final_dir);
    strncpy(config.rootfs, final_rootfs, sizeof(config.rootfs) - 1);

    // Container oluştur
    char id[17];
    memset(id, 0, sizeof(id));
    if (container_create(&config, id, sizeof(id)) != 0) {
        fprintf(stderr, "image_import: container olusturulamadi\n");
        return -1;
    }
    printf("İmajınız yüklendi.\n");
    printf("Calistirmak icin: koza run -i %s\n", id);
    return 0;
}
