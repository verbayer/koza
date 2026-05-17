#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/cli.h"
#include "../include/container.h"
#include "../include/config.h"

static void print_usage(void) {
    printf("Kullanim:\n");
    printf("  koza create --name <isim> --rootfs <path> --cmd <komut> [opsiyonlar]\n");
    printf("  koza run <id>\n");
    printf("  koza stop <id>\n");
    printf("  koza delete <id>\n");
    printf("  koza list\n");
    printf("\nOpsiyonlar:\n");
    printf("  --name      <isim>        Container ismi\n");
    printf("  --rootfs    <path>        Rootfs dizini\n");
    printf("  --cmd       <komut>       Calistirilacak komut\n");
    printf("  --memory    <bytes>       Bellek limiti\n");
    printf("  --cpu-quota <us>          CPU kotasi (mikrosaniye)\n");
    printf("  --cpu-period<us>          CPU periyodu (mikrosaniye, default: 100000)\n");
    printf("  --pids      <limit>       Max process sayisi\n");
    printf("  --caps      <caps>        Capability string\n");
    printf("  --rootless               Rootless container\n");
}

static int cmd_create(int argc, char **argv) {
    container_config_t config;
    memset(&config, 0, sizeof(config));

    // Default değerler
    config.cgroup.cpu_period = 100000;

    static struct option long_opts[] = {
        { "name",       required_argument, 0, 'n' },
        { "rootfs",     required_argument, 0, 'r' },
        { "cmd",        required_argument, 0, 'c' },
        { "memory",     required_argument, 0, 'm' },
        { "cpu-quota",  required_argument, 0, 'q' },
        { "cpu-period", required_argument, 0, 'p' },
        { "pids",       required_argument, 0, 'P' },
        { "caps",       required_argument, 0, 'C' },
        { "rootless",   no_argument,       0, 'L' },
        { "config", required_argument, 0, 'f' },
	{ "uid", required_argument, 0, 'u' },
        { "gid", required_argument, 0, 'g' },
	{ 0, 0, 0, 0 }
    };

    optind = 2;
    int c;
    while ((c = getopt_long(argc, argv, "u:g:n:r:c:m:q:p:P:C:Lf:", long_opts, NULL)) != -1) {
        switch (c) {
            case 'n':
                strncpy(config.name, optarg, sizeof(config.name) - 1);
                break;
            case 'r':
                strncpy(config.rootfs, optarg, sizeof(config.rootfs) - 1);
                break;
            case 'c':
                strncpy(config.command, optarg, sizeof(config.command) - 1);
                break;
            case 'm':
                config.cgroup.memory_limit = atol(optarg);
                break;
            case 'q':
                config.cgroup.cpu_quota = atol(optarg);
                break;
            case 'p':
                config.cgroup.cpu_period = atol(optarg);
                break;
            case 'P':
                config.cgroup.pids_limit = atol(optarg);
                break;
            case 'C':
                strncpy(config.capabilities, optarg, sizeof(config.capabilities) - 1);
                break;
            case 'L':
                config.rootless = 1;
                break;
	    case 'u':
    	        config.uid = (uid_t)atoi(optarg);
    		break;
		case 'g':
    		config.gid = (gid_t)atoi(optarg);
   		 break;
	    case 'f':
		if (config_load(optarg, &config) != 0)
        	return -1;
   		break;
            case '?':
                print_usage();
                return -1;
        }
    }

    // Zorunlu alanları kontrol et
    if (strlen(config.name) == 0) {
        fprintf(stderr, "Hata: --name zorunlu\n");
        return -1;
    }
    if (strlen(config.rootfs) == 0) {
        fprintf(stderr, "Hata: --rootfs zorunlu\n");
        return -1;
    }
    if (strlen(config.command) == 0) {
        fprintf(stderr, "Hata: --cmd zorunlu\n");
        return -1;
    }

    // Hostname'i name'den al
    strncpy(config.hostname, config.name, sizeof(config.hostname) - 1);

    char id[17];
    memset(id, 0, sizeof(id));
    if (container_create(&config, id, sizeof(id)) != 0)
        return -1;

    printf("Container ID: %s\n", id);
    return 0;
}

static int cmd_run(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Kullanim: koza run [-i] <id>\n");
        return -1;
    }

    int interactive = 0;
    const char *id;

    if (strcmp(argv[2], "-i") == 0 || strcmp(argv[2], "--interactive") == 0) {
        interactive = 1;
        if (argc < 4) {
            fprintf(stderr, "Kullanim: koza run -i <id>\n");
            return -1;
        }
        id = argv[3];
    } else {
        id = argv[2];
    }

    return container_run(id, interactive);

}

static int cmd_stop(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Kullanim: koza stop <id>\n");
        return -1;
    }
    return container_stop(argv[2]);
}

static int cmd_delete(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Kullanim: koza delete <id>\n");
        return -1;
    }
    return container_delete(argv[2]);
}
static int cmd_config(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[2], "init") != 0) {
        fprintf(stderr, "Kullanim: koza config init <isim>\n");
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s.json", argv[3]);
    return config_init(argv[3], path);
}

int cli_run(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return -1;
    }

    if (strcmp(argv[1], "create") == 0)
        return cmd_create(argc, argv);
    if (strcmp(argv[1], "run") == 0)
        return cmd_run(argc, argv);
    if (strcmp(argv[1], "stop") == 0)
        return cmd_stop(argc, argv);
    if (strcmp(argv[1], "delete") == 0)
        return cmd_delete(argc, argv);
    if (strcmp(argv[1], "list") == 0)
        return container_list();
    if (strcmp(argv[1], "config") == 0)
        return cmd_config(argc, argv);
    fprintf(stderr, "Bilinmeyen komut: %s\n", argv[1]);
    print_usage();
    return -1;
}
