/* tcpecho.c - userland: open a TCP connection, send a line, and verify the
 * echo comes back. In CI this reaches a host echo server via SLIRP guestfwd
 * at 10.0.2.100:9. */
#include "ulib.h"

void _start(void) {
    unsigned int ip = (10u << 24) | (0u << 16) | (2u << 8) | 100u; /* 10.0.2.100 */
    if (sys_tcp_connect(ip, 9) != 0) {
        uprintf("tcp: connect failed\n");
        sys_exit(1);
    }

    const char *msg = "hello over tcp";
    int mlen = 14;
    sys_tcp_send(msg, mlen);

    char buf[64];
    int total = 0;
    /* The echo may arrive in pieces; read until we have the whole line. */
    while (total < mlen) {
        int n = sys_tcp_recv(buf + total, (int)sizeof(buf) - total);
        if (n <= 0)
            break;
        total += n;
    }
    sys_tcp_close();

    if (total == mlen && ustrncmp(buf, msg, (unsigned)mlen) == 0)
        uprintf("tcp: echo ok (%d bytes)\n", total);
    else
        uprintf("tcp: echo mismatch (got %d bytes)\n", total);
    sys_exit(0);
}
