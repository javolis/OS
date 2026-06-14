/* netcap.c - networking capstone: exercise the whole stack from ring 3 in
 * one program (DHCP, config read, ICMP ping, DNS, TCP echo) and tally the
 * result, like runtests.elf for the syscall surface. Prints
 * "netcap: ALL PASS (N tests)" only if every check succeeds. */
#include "ulib.h"

static int pass, total;

static void check(int cond, const char *name) {
    total++;
    if (cond)
        pass++;
    else
        uprintf("netcap: FAIL %s\n", name);
}

void _start(void) {
    /* DHCP: obtain a lease. */
    unsigned int leased = sys_dhcp();
    check(leased != 0, "dhcp lease");

    /* Read back the configuration. */
    struct netinfo ni;
    int have_cfg = (sys_netinfo(&ni) == 0 && ni.ip != 0);
    check(have_cfg, "netinfo");

    /* ICMP ping the gateway. */
    unsigned int gw = have_cfg ? ni.gateway : ((10u << 24) | (2u << 8) | 2u);
    check(sys_ping(gw) >= 0, "ping gateway");

    /* DNS resolve a name. */
    unsigned int rip = 0;
    check(sys_resolve("example.com", &rip) == 0 && rip != 0, "dns resolve");

    /* TCP echo via the host echo server (SLIRP guestfwd 10.0.2.100:9). */
    unsigned int echo_ip = (10u << 24) | (2u << 8) | 100u;
    int tcp_ok = 0;
    if (sys_tcp_connect(echo_ip, 9) == 0) {
        const char *m = "netcap-tcp";
        int ml = 10;
        sys_tcp_send(m, ml);
        char b[32];
        int got = 0;
        while (got < ml) {
            int n = sys_tcp_recv(b + got, (int)sizeof(b) - got);
            if (n <= 0)
                break;
            got += n;
        }
        sys_tcp_close();
        tcp_ok = (got == ml && ustrncmp(b, m, (unsigned)ml) == 0);
    }
    check(tcp_ok, "tcp echo");

    if (pass == total)
        uprintf("netcap: ALL PASS (%d tests)\n", total);
    else
        uprintf("netcap: %d/%d passed\n", pass, total);
    sys_exit(pass == total ? 0 : 1);
}
