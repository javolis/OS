/* dhcp.c - userland: obtain an IPv4 lease via DHCP. */
#include "ulib.h"

void _start(void) {
    unsigned int ip = sys_dhcp();
    if (ip == 0) {
        uprintf("dhcp: no lease\n");
        sys_exit(1);
    }
    uprintf("dhcp: leased %u.%u.%u.%u\n", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF, ip & 0xFF);
    sys_exit(0);
}
