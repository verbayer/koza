#ifndef CONFIG_H
#define CONFIG_H

#include "container.h"

int config_load(const char *path, container_config_t *config);
int config_init(const char *name, const char *path);

#endif
