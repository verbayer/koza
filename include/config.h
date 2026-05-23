#ifndef CONFIG_H
#define CONFIG_H

#include "container.h"

int config_load(const char *path, container_config_t *config); //container oluşturmada config verilirse yükle
int config_init(const char *name, const char *path); //örnek config dosyası oluştur
int config_save(const char *path, container_config_t *config); //container oluştururken kullanılan config'i container dizinine kaydet
#endif
