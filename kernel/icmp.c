/* icmp.c - ICMP echo request/reply (ping).
 *
 * icmp_ping (syscall context) builds an echo request, sends it, and sleeps
 * in tick slices until the matching reply is seen by icmp_rx (IRQ context),
 * or a timeout fires. We also answer echo requests addressed to us. ICMP
 * messages are built byte by byte in network order to keep the checksum
 * unambiguous. */
#include <stddef.h>
#include <stdint.h>

#include "icmp.h"
#include "ip.h"
#include "net.h"
#include "sched.h"
#include "timer.h"

#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

#define PING_TIMEOUT_TICKS 100 /* ~1 second at 100 Hz */

static volatile int ping_pending;
static volatile int ping_got;
static uint16_t exp_id;
static uint16_t exp_seq;

static void icmp_rx(ipaddr_t src, const uint8_t *payload, uint16_t len) {
    if (len < 8)
        return;
    uint8_t type = payload[0];
    if (type == ICMP_ECHO_REPLY) {
        uint16_t rid = (uint16_t)((payload[4] << 8) | payload[5]);
        uint16_t rseq = (uint16_t)((payload[6] << 8) | payload[7]);
        if (ping_pending && rid == exp_id && rseq == exp_seq)
            ping_got = 1;
        return;
    }
    if (type == ICMP_ECHO_REQUEST) {
        /* Someone is pinging us: echo it back as a reply. */
        uint8_t buf[1500];
        if (len > sizeof(buf))
            return;
        for (uint16_t i = 0; i < len; i++)
            buf[i] = payload[i];
        buf[0] = ICMP_ECHO_REPLY;
        buf[2] = 0;
        buf[3] = 0;
        uint16_t c = ip_checksum(buf, len);
        buf[2] = (uint8_t)(c >> 8);
        buf[3] = (uint8_t)(c & 0xFF);
        ip_send(src, IPPROTO_ICMP, buf, len);
    }
}

void icmp_init(void) {
    ip_register(IPPROTO_ICMP, icmp_rx);
}

int icmp_ping(ipaddr_t dst) {
    static uint16_t seqc;
    uint8_t pkt[40];
    const uint16_t total = 40; /* 8-byte header + 32 bytes of data */

    exp_id = 0x4f53; /* 'OS' */
    exp_seq = (uint16_t)(++seqc);

    pkt[0] = ICMP_ECHO_REQUEST;
    pkt[1] = 0;
    pkt[2] = 0; /* checksum, filled below */
    pkt[3] = 0;
    pkt[4] = (uint8_t)(exp_id >> 8);
    pkt[5] = (uint8_t)exp_id;
    pkt[6] = (uint8_t)(exp_seq >> 8);
    pkt[7] = (uint8_t)exp_seq;
    for (int i = 0; i < 32; i++)
        pkt[8 + i] = (uint8_t)i;
    uint16_t c = ip_checksum(pkt, total);
    pkt[2] = (uint8_t)(c >> 8);
    pkt[3] = (uint8_t)(c & 0xFF);

    ping_got = 0;
    ping_pending = 1;
    uint32_t start = timer_ticks();
    if (ip_send(dst, IPPROTO_ICMP, pkt, total) != 0) {
        ping_pending = 0;
        return -1; /* next hop not resolved */
    }
    while (!ping_got && (timer_ticks() - start) < PING_TIMEOUT_TICKS)
        sched_sleep_current(1); /* yield; the reply arrives via the RX IRQ */
    ping_pending = 0;
    if (!ping_got)
        return -1;
    return (int)(timer_ticks() - start);
}
