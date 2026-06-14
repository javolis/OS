/* nslookup.c - userland: resolve a hostname to an IPv4 address via DNS.
 * Usage: nslookup [hostname]   (defaults to example.com) */
#include "ulib.h"

void _start(int argc, char **argv) {
    const char *name = (argc >= 2) ? argv[1] : "example.com";
    unsigned int ip;
    if (sys_resolve(name, &ip) == 0) {
        uprintf("dns: %s -> %u.%u.%u.%u\n", name, (ip >> 24) & 0xFF,
                (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
        sys_exit(0);
    }
    uprintf("dns: could not resolve %s\n", name);
    sys_exit(1);
}
