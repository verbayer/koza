#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include "../include/cli.h"
#include "../include/network.h"
#include "../include/utils.h"

int main(int argc, char **argv) {
    // Gerekli dizinleri oluştur
    if (mkdir_p("/var/lib/koza/containers", 0755) != 0) {
        fprintf(stderr, "Dizin olusturulamadi\n");
        return 1;
    }


    // koza0 bridge zaten varsa network_init'i atla
    if (if_nametoindex("koza0") == 0) {
        if (network_init() != 0) {
            fprintf(stderr, "Network baslatılamadi\n");
            return 1;
        }
    }


    // CLI'ya yönlendir
    return cli_run(argc, argv) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
