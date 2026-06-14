/* udp.c - UDP send/receive over IPv4.
 *
 * Datagrams go out with a zero checksum (legal for IPv4 UDP and accepted by
 * SLIRP), which keeps the code simple. A blocking request/reply helper
 * (one transaction at a time) sends a datagram and sleeps until a reply to
 * our source port arrives via the RX IRQ - the same model as ping. */
#include <stddef.h>
#include <stdint.h>

#include "ip.h"
#include "kprintf.h"
#include "net.h"
#include "sched.h"
#include "timer.h"
#include "udp.h"

#define UDP_TIMEOUT_TICKS 200 /* ~2 seconds */

static volatile int udp_pending;
static volatile int udp_got;
static uint16_t want_port;
static uint8_t rx_store[1500];
static volatile uint16_t rx_len;
static int udp_logged;

#define UDP_LISTENERS 4
static struct {
    uint16_t port;
    udp_listener_fn fn;
} listeners[UDP_LISTENERS];
static int nlisteners;

void udp_listen(uint16_t port, udp_listener_fn fn) {
    if (nlisteners < UDP_LISTENERS) {
        listeners[nlisteners].port = port;
        listeners[nlisteners].fn = fn;
        nlisteners++;
    }
}

int udp_send(ipaddr_t dst, uint16_t sport, uint16_t dport, const void *data,
             uint16_t len) {
    uint8_t pkt[1500];
    if ((int)len + 8 > (int)sizeof(pkt))
        return -1;
    uint16_t ulen = (uint16_t)(8 + len);
    pkt[0] = (uint8_t)(sport >> 8);
    pkt[1] = (uint8_t)sport;
    pkt[2] = (uint8_t)(dport >> 8);
    pkt[3] = (uint8_t)dport;
    pkt[4] = (uint8_t)(ulen >> 8);
    pkt[5] = (uint8_t)ulen;
    pkt[6] = 0; /* checksum 0: not computed (allowed for IPv4 UDP) */
    pkt[7] = 0;
    const uint8_t *d = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++)
        pkt[8 + i] = d[i];
    return ip_send(dst, IPPROTO_UDP, pkt, ulen);
}

static void udp_rx(ipaddr_t src, const uint8_t *payload, uint16_t len) {
    if (len < 8)
        return;
    uint16_t sport = (uint16_t)((payload[0] << 8) | payload[1]);
    uint16_t dport = (uint16_t)((payload[2] << 8) | payload[3]);
    uint16_t ulen = (uint16_t)((payload[4] << 8) | payload[5]);
    if (ulen < 8 || ulen > len)
        ulen = len; /* tolerate padding / odd lengths */
    uint16_t dlen = (uint16_t)(ulen - 8);
    if (udp_logged < 16) {
        kprintf("udp: rx %u bytes from %u.%u.%u.%u:%u\n", dlen,
                (src >> 24) & 0xFF, (src >> 16) & 0xFF, (src >> 8) & 0xFF,
                src & 0xFF, sport);
        udp_logged++;
    }
    if (udp_pending && dport == want_port) {
        uint16_t n = dlen > sizeof(rx_store) ? (uint16_t)sizeof(rx_store) : dlen;
        for (uint16_t i = 0; i < n; i++)
            rx_store[i] = payload[8 + i];
        rx_len = n;
        udp_got = 1;
    }
    for (int i = 0; i < nlisteners; i++)
        if (listeners[i].port == dport)
            listeners[i].fn(src, payload + 8, dlen);
}

void udp_init(void) {
    ip_register(IPPROTO_UDP, udp_rx);
}

int udp_request(ipaddr_t dst, uint16_t dport, uint16_t sport,
                const void *txdata, uint16_t txlen, uint8_t *rxbuf,
                uint16_t rxmax) {
    want_port = sport;
    udp_got = 0;
    rx_len = 0;
    udp_pending = 1;
    uint32_t start = timer_ticks();
    /* The first send may miss the ARP cache for an on-subnet host; the call
     * fires a request, so retry across a tick to let the reply cache. */
    int sent = -1;
    for (int t = 0; t < 10 && sent != 0; t++) {
        sent = udp_send(dst, sport, dport, txdata, txlen);
        if (sent != 0)
            sched_sleep_current(1);
    }
    if (sent != 0) {
        udp_pending = 0;
        return -1;
    }
    while (!udp_got && (timer_ticks() - start) < UDP_TIMEOUT_TICKS)
        sched_sleep_current(1);
    udp_pending = 0;
    if (!udp_got)
        return -1;
    uint16_t n = rx_len > rxmax ? rxmax : rx_len;
    for (uint16_t i = 0; i < n; i++)
        rxbuf[i] = rx_store[i];
    return (int)n;
}
