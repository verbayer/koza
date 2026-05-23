#ifndef IMAGE_H
#define IMAGE_H

#define IMAGE_BASE_PATH "/var/lib/koza/images"

int image_export(const char *container_id);
int image_import(const char *archive_path);

#endif
