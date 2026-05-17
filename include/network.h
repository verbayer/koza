#ifndef NETWORK_H
#define NETWORK_H

#include <sys/types.h>

#define KOZA_BRIDGE  "koza0"
#define KOZA_SUBNET  "10.0.0"
#define KOZA_GATEWAY "10.0.0.1"

typedef struct {
    char veth_host[16];   // host tarafındaki veth, örn. "veth0a1b2c"
    char veth_cont[16];   // container tarafındaki veth, örn. "eth0"
    char ip[16];          // container IP'si, örn. "10.0.0.2"
    char gateway[16];     // gateway IP'si
} network_config_t;

int network_init(void);                                    // bridge oluştur, NAT kur
int network_setup_container(pid_t pid, network_config_t *cfg);  // veth oluştur, IP ata
int network_cleanup(const char *veth_host);               // veth sil

#endif
