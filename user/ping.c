/* ping.c - userland: ICMP echo to an IPv4 address via sys_ping.
 * Usage: ping [a.b.c.d]   (defaults to the SLIRP gateway 10.0.2.2) */
#include "ulib.h"

/* Parse dotted-decimal into a host-order u32. Returns 1 on success. */
static int parse_ip(const char *s, unsigned *out) {
    unsigned v = 0;
    int parts = 0, oct = 0, have = 0;
    for (const char *p = s;; p++) {
        if (*p >= '0' && *p <= '9') {
            oct = oct * 10 + (*p - '0');
            have = 1;
        } else if (*p == '.' || *p == '\0') {
            if (!have || oct > 255)
                return 0;
            v = (v << 8) | (unsigned)oct;
            parts++;
            oct = 0;
            have = 0;
            if (*p == '\0')
                break;
        } else {
            return 0;
        }
    }
    if (parts != 4)
        return 0;
    *out = v;
    return 1;
}

void _start(int argc, char **argv) {
    const char *target = (argc >= 2) ? argv[1] : "10.0.2.2";
    unsigned ip;
    if (!parse_ip(target, &ip)) {
        uprintf("ping: bad address %s\n", target);
        sys_exit(1);
    }
    int ms = sys_ping(ip);
    if (ms < 0)
        uprintf("ping: no reply from %s\n", target);
    else
        uprintf("ping: reply from %s in %d ms\n", target, ms);
    sys_exit(ms < 0 ? 1 : 0);
}
