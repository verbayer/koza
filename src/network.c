#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/veth.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <netlink/route/nexthop.h>
#include <arpa/inet.h>
#include "../include/network.h"
#include "../include/utils.h"
#include <netlink/route/link/bridge.h>  // rtnl_link_bridge_alloc için
// Netlink socket aç
static struct nl_sock *open_socket(void) {
    struct nl_sock *sock = nl_socket_alloc();
    if (!sock) {
        fprintf(stderr, "open_socket: nl_socket_alloc basarisiz\n");
        return NULL;
    }
    if (nl_connect(sock, NETLINK_ROUTE) != 0) {
        fprintf(stderr, "open_socket: nl_connect basarisiz\n");
        nl_socket_free(sock);
        return NULL;
    }
    return sock;
}

// Arayüzü ayağa kaldır
static int link_up(struct nl_sock *sock, const char *ifname) {
    struct nl_cache *cache;
    if (rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache) != 0) {
        fprintf(stderr, "link_up: cache alinamadi\n");
        return -1;
    }

    struct rtnl_link *link = rtnl_link_get_by_name(cache, ifname);
    if (!link) {
        fprintf(stderr, "link_up: %s bulunamadi\n", ifname);
        nl_cache_free(cache);
        return -1;
    }

    struct rtnl_link *change = rtnl_link_alloc();
    rtnl_link_set_flags(change, IFF_UP);

    int ret = rtnl_link_change(sock, link, change, 0);
    rtnl_link_put(link);
    rtnl_link_put(change);
    nl_cache_free(cache);

    if (ret != 0) {
        fprintf(stderr, "link_up: %s ayaga kaldirilirken hata: %s\n", ifname, nl_geterror(ret));
        return -1;
    }
    return 0;
}

// Arayüze IP ata
static int assign_ip(struct nl_sock *sock, const char *ifname, const char *ip, int prefix_len) {
    struct nl_cache *cache;
    if (rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache) != 0)
        return -1;

    struct rtnl_link *link = rtnl_link_get_by_name(cache, ifname);
    if (!link) {
        nl_cache_free(cache);
        return -1;
    }
    int ifindex = rtnl_link_get_ifindex(link);
    rtnl_link_put(link);
    nl_cache_free(cache);

    struct rtnl_addr *addr = rtnl_addr_alloc();
    struct nl_addr *local;

    char cidr[32];
    snprintf(cidr, sizeof(cidr), "%s/%d", ip, prefix_len);
    if (nl_addr_parse(cidr, AF_INET, &local) != 0) {
        fprintf(stderr, "assign_ip: IP parse edilemedi: %s\n", cidr);
        rtnl_addr_put(addr);
        return -1;
    }

    rtnl_addr_set_ifindex(addr, ifindex);
    rtnl_addr_set_local(addr, local);
    nl_addr_put(local);

    int ret = rtnl_addr_add(sock, addr, NLM_F_CREATE);
    rtnl_addr_put(addr);

    if (ret != 0 && ret != -NLE_EXIST) {
        fprintf(stderr, "assign_ip: %s icin IP atanamadi: %s\n", ifname, nl_geterror(ret));
        return -1;
    }
    return 0;
}

// Default route ekle
static int add_default_route(struct nl_sock *sock, const char *gateway_ip) {
    struct rtnl_route *route = rtnl_route_alloc();
    struct rtnl_nexthop *nh  = rtnl_route_nh_alloc();

    // 0.0.0.0/0 — default route
    struct nl_addr *dst;
    nl_addr_parse("0.0.0.0/0", AF_INET, &dst);
    rtnl_route_set_dst(route, dst);
    nl_addr_put(dst);

    struct nl_addr *gw;
    nl_addr_parse(gateway_ip, AF_INET, &gw);
    rtnl_route_nh_set_gateway(nh, gw);
    nl_addr_put(gw);

    rtnl_route_add_nexthop(route, nh);

    int ret = rtnl_route_add(sock, route, NLM_F_CREATE);
    rtnl_route_put(route);

    if (ret != 0 && ret != -NLE_EXIST) {
        fprintf(stderr, "add_default_route: route eklenemedi: %s\n", nl_geterror(ret));
        return -1;
    }
    return 0;
}

int network_init(void) {
    struct nl_sock *sock = open_socket();
    if (!sock) return -1;

    // 1. Bridge oluştur
    struct rtnl_link *bridge = rtnl_link_bridge_alloc();
    if (!bridge) {
        fprintf(stderr, "network_init: bridge alloc basarisiz\n");
        nl_socket_free(sock);
        return -1;
    }
    rtnl_link_set_name(bridge, KOZA_BRIDGE);

    int ret = rtnl_link_add(sock, bridge, NLM_F_CREATE);
    rtnl_link_put(bridge);

    if (ret != 0 && ret != -NLE_EXIST) {
        fprintf(stderr, "network_init: bridge olusturulamadi: %s\n", nl_geterror(ret));
        nl_socket_free(sock);
        return -1;
    }

    // 2. Bridge'e IP ata (gateway)
    if (assign_ip(sock, KOZA_BRIDGE, KOZA_GATEWAY, 24) != 0) {
        nl_socket_free(sock);
        return -1;
    }

    // 3. Bridge'i ayağa kaldır
    if (link_up(sock, KOZA_BRIDGE) != 0) {
        nl_socket_free(sock);
        return -1;
    }

    nl_socket_free(sock);

    // 4. NAT kural ekle (iptables)
    int r = system("iptables -t nat -C POSTROUTING -s 10.0.0.0/24 -j MASQUERADE 2>/dev/null"
                   " || iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE");
    if (r != 0) {
        fprintf(stderr, "network_init: iptables NAT kurulamadi\n");
        return -1;
    }

    // 5. IP forwarding'i etkinleştir
    if (write_file("/proc/sys/net/ipv4/ip_forward", "1") != 0) {
        fprintf(stderr, "network_init: ip_forward etkinlestirilemedi\n");
        return -1;
    }

    printf("Network init tamamlandi: %s (%s)\n", KOZA_BRIDGE, KOZA_GATEWAY);
    return 0;
}

int network_setup_container(pid_t pid, network_config_t *cfg) {
    if (!cfg) {
        fprintf(stderr, "network_setup_container: cfg NULL\n");
        return -1;
    }

    struct nl_sock *sock = open_socket();
    if (!sock) return -1;

    // 1. Veth çifti oluştur
    struct rtnl_link *veth = rtnl_link_veth_alloc();
    if (!veth) {
        fprintf(stderr, "network_setup_container: veth alloc basarisiz\n");
        nl_socket_free(sock);
        return -1;
    }

    rtnl_link_set_name(veth, cfg->veth_host);
    struct rtnl_link *peer = rtnl_link_veth_get_peer(veth);
    rtnl_link_set_name(peer, cfg->veth_cont);

    int ret = rtnl_link_add(sock, veth, NLM_F_CREATE);
    rtnl_link_put(veth);

    if (ret != 0) {
        fprintf(stderr, "network_setup_container: veth olusturulamadi: %s\n", nl_geterror(ret));
        nl_socket_free(sock);
        return -1;
    }

    // 2. Host tarafını bridge'e bağla
    struct nl_cache *cache;
    rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);

    struct rtnl_link *host_veth = rtnl_link_get_by_name(cache, cfg->veth_host);
    struct rtnl_link *br        = rtnl_link_get_by_name(cache, KOZA_BRIDGE);

    if (!host_veth || !br) {
        fprintf(stderr, "network_setup_container: veth veya bridge bulunamadi\n");
        nl_cache_free(cache);
        nl_socket_free(sock);
        return -1;
    }

    ret = rtnl_link_enslave(sock, br, host_veth);
    rtnl_link_put(host_veth);
    rtnl_link_put(br);
    nl_cache_free(cache);

    if (ret != 0) {
        fprintf(stderr, "network_setup_container: bridge'e baglanamadi: %s\n", nl_geterror(ret));
        nl_socket_free(sock);
        return -1;
    }

    // 3. Host tarafını ayağa kaldır
    if (link_up(sock, cfg->veth_host) != 0) {
        nl_socket_free(sock);
        return -1;
    }

    // 4. Container tarafını container network namespace'ine taşı
    rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);
    struct rtnl_link *cont_veth = rtnl_link_get_by_name(cache, cfg->veth_cont);
    if (!cont_veth) {
        fprintf(stderr, "network_setup_container: container veth bulunamadi\n");
        nl_cache_free(cache);
        nl_socket_free(sock);
        return -1;
    }

    struct rtnl_link *change = rtnl_link_alloc();
    rtnl_link_set_ns_pid(change, pid);

    ret = rtnl_link_change(sock, cont_veth, change, 0);
    rtnl_link_put(cont_veth);
    rtnl_link_put(change);
    nl_cache_free(cache);

    if (ret != 0) {
        fprintf(stderr, "network_setup_container: namespace'e tasinamadi: %s\n", nl_geterror(ret));
        nl_socket_free(sock);
        return -1;
    }

    nl_socket_free(sock);

    // 5. Container namespace'ine gir, IP ve route ayarla
    char ns_path[64];
    snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/net", pid);

    int ns_fd = open(ns_path, O_RDONLY);
    if (ns_fd == -1) {
        perror("network_setup_container: ns open");
        return -1;
    }

    // Mevcut namespace'i kaydet
    int old_ns_fd = open("/proc/self/ns/net", O_RDONLY);
    if (old_ns_fd == -1) {
        perror("network_setup_container: old ns open");
        close(ns_fd);
        return -1;
    }

    // Container namespace'ine geç
    if (setns(ns_fd, CLONE_NEWNET) == -1) {
        perror("network_setup_container: setns");
        close(ns_fd);
        close(old_ns_fd);
        return -1;
    }
    close(ns_fd);

    // Container namespace içinde netlink socket aç
    struct nl_sock *cont_sock = open_socket();
    if (!cont_sock) {
        setns(old_ns_fd, CLONE_NEWNET);
        close(old_ns_fd);
        return -1;
    }

    // IP ata
    if (assign_ip(cont_sock, cfg->veth_cont, cfg->ip, 24) != 0) {
        nl_socket_free(cont_sock);
        setns(old_ns_fd, CLONE_NEWNET);
        close(old_ns_fd);
        return -1;
    }

    // Arayüzü ayağa kaldır
    if (link_up(cont_sock, cfg->veth_cont) != 0) {
        nl_socket_free(cont_sock);
        setns(old_ns_fd, CLONE_NEWNET);
        close(old_ns_fd);
        return -1;
    }

    // Loopback'i ayağa kaldır
    link_up(cont_sock, "lo");

    // Default route ekle
    if (add_default_route(cont_sock, cfg->gateway) != 0) {
        nl_socket_free(cont_sock);
        setns(old_ns_fd, CLONE_NEWNET);
        close(old_ns_fd);
        return -1;
    }

    nl_socket_free(cont_sock);

    // Eski namespace'e geri dön
    if (setns(old_ns_fd, CLONE_NEWNET) == -1) {
        perror("network_setup_container: eski namespace'e donemedi");
        close(old_ns_fd);
        return -1;
    }
    close(old_ns_fd);

    printf("Network kuruldu: %s <-> %s (%s)\n", cfg->veth_host, cfg->veth_cont, cfg->ip);
    return 0;
}

int network_cleanup(const char *veth_host) {
    if (!veth_host) {
        fprintf(stderr, "network_cleanup: veth_host NULL\n");
        return -1;
    }

    struct nl_sock *sock = open_socket();
    if (!sock) return -1;

    struct nl_cache *cache;
    rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);

    struct rtnl_link *link = rtnl_link_get_by_name(cache, veth_host);
    if (!link) {
        // Zaten silinmiş olabilir
        nl_cache_free(cache);
        nl_socket_free(sock);
        return 0;
    }

    int ret = rtnl_link_delete(sock, link);
    rtnl_link_put(link);
    nl_cache_free(cache);
    nl_socket_free(sock);

    if (ret != 0) {
        fprintf(stderr, "network_cleanup: silinemedi: %s\n", nl_geterror(ret));
        return -1;
    }

    return 0;
}
