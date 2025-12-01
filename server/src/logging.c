#include "logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("[SERVER] ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

void log_server_lan_ip() {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    printf("[SERVER] LAN IPs:");
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sa->sin_addr), ip, INET_ADDRSTRLEN);
            if (strcmp(ip, "127.0.0.1") != 0) {
                printf("  %s: %s", ifa->ifa_name, ip);
            }
        }
    }
    printf("\n");
    freeifaddrs(ifaddr);
}

void die(const char *msg) {
    perror(msg);
    exit(1);
}
