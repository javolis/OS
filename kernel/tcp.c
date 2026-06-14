/* tcp.c - a deliberately minimal TCP client: one connection at a time, no
 * retransmission, no window management beyond a fixed advertised window.
 * Enough to connect, exchange a little data with a well-behaved peer
 * (SLIRP), and close. The driving calls (connect/send/recv/close) run in
 * syscall context and sleep while waiting; tcp_rx runs in IRQ context,
 * updates the connection block, stores received data, and ACKs it.
 *
 * TCP requires a real checksum (computed over an IPv4 pseudo-header plus the
 * segment), unlike UDP. Send buffers are static: syscalls run with
 * interrupts off, so a syscall-context send never overlaps an IRQ-context
 * ACK. */
#include <stddef.h>
#include <stdint.h>

#include "ip.h"
#include "net.h"
#include "sched.h"
#include "tcp.h"
#include "timer.h"

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

enum { CLOSED, SYN_SENT, ESTABLISHED, CLOSING };

#define TCP_WINDOW 4096
#define TCP_RXBUF 4096
#define TCP_MAXDATA 1024

static struct {
    int state;
    ipaddr_t peer_ip;
    uint16_t peer_port, local_port;
    uint32_t snd_nxt, rcv_nxt;
    volatile int got_synack, peer_fin;
    uint8_t rxbuf[TCP_RXBUF];
    volatile uint16_t rxlen;
} tcb;

static uint32_t isn_base = 10000;

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Build and send one segment with the current snd_nxt/rcv_nxt. Returns 0 on
 * success, -1 if the next hop isn't resolved yet. */
static int send_segment(uint8_t flags, const uint8_t *data, uint16_t dlen) {
    static uint8_t seg[20 + TCP_MAXDATA];
    static uint8_t pseudo[12 + 20 + TCP_MAXDATA];
    if (dlen > TCP_MAXDATA)
        dlen = TCP_MAXDATA;
    seg[0] = (uint8_t)(tcb.local_port >> 8);
    seg[1] = (uint8_t)tcb.local_port;
    seg[2] = (uint8_t)(tcb.peer_port >> 8);
    seg[3] = (uint8_t)tcb.peer_port;
    wr32(seg + 4, tcb.snd_nxt);
    wr32(seg + 8, tcb.rcv_nxt);
    seg[12] = 5 << 4; /* data offset = 5 dwords (20 bytes) */
    seg[13] = flags;
    seg[14] = (uint8_t)(TCP_WINDOW >> 8);
    seg[15] = (uint8_t)TCP_WINDOW;
    seg[16] = 0; /* checksum */
    seg[17] = 0;
    seg[18] = 0; /* urgent */
    seg[19] = 0;
    for (uint16_t i = 0; i < dlen; i++)
        seg[20 + i] = data[i];
    uint16_t seglen = (uint16_t)(20 + dlen);

    /* Pseudo-header + segment for the checksum. */
    wr32(pseudo, net_ip());
    wr32(pseudo + 4, tcb.peer_ip);
    pseudo[8] = 0;
    pseudo[9] = IPPROTO_TCP;
    pseudo[10] = (uint8_t)(seglen >> 8);
    pseudo[11] = (uint8_t)seglen;
    for (uint16_t i = 0; i < seglen; i++)
        pseudo[12 + i] = seg[i];
    uint16_t c = ip_checksum(pseudo, 12 + seglen);
    seg[16] = (uint8_t)(c >> 8);
    seg[17] = (uint8_t)(c & 0xFF);

    return ip_send(tcb.peer_ip, IPPROTO_TCP, seg, seglen);
}

static void tcp_rx(ipaddr_t src, const uint8_t *seg, uint16_t len) {
    if (len < 20 || tcb.state == CLOSED)
        return;
    uint16_t sport = (uint16_t)((seg[0] << 8) | seg[1]);
    uint16_t dport = (uint16_t)((seg[2] << 8) | seg[3]);
    if (src != tcb.peer_ip || sport != tcb.peer_port ||
        dport != tcb.local_port)
        return;
    uint32_t their_seq = rd32(seg + 4);
    uint8_t flags = seg[13];
    uint16_t doff = (uint16_t)((seg[12] >> 4) * 4);
    if (doff < 20 || doff > len)
        return;
    uint16_t dlen = (uint16_t)(len - doff);

    if (flags & TCP_RST) {
        tcb.state = CLOSING;
        tcb.peer_fin = 1;
        return;
    }

    if (tcb.state == SYN_SENT && (flags & TCP_SYN) && (flags & TCP_ACK)) {
        tcb.rcv_nxt = their_seq + 1; /* SYN consumes one sequence number */
        tcb.got_synack = 1;
        return;
    }

    if (tcb.state == ESTABLISHED) {
        /* In-order data only (no reassembly). ACK what we accept. */
        if (dlen > 0 && their_seq == tcb.rcv_nxt) {
            const uint8_t *d = seg + doff;
            for (uint16_t i = 0; i < dlen && tcb.rxlen < TCP_RXBUF; i++)
                tcb.rxbuf[tcb.rxlen++] = d[i];
            tcb.rcv_nxt += dlen;
            send_segment(TCP_ACK, NULL, 0);
        }
        if (flags & TCP_FIN) {
            tcb.rcv_nxt += 1; /* FIN consumes one sequence number */
            tcb.peer_fin = 1;
            send_segment(TCP_ACK, NULL, 0);
        }
    }
}

void tcp_init(void) {
    ip_register(IPPROTO_TCP, tcp_rx);
}

int tcp_connect(ipaddr_t ip, uint16_t port) {
    tcb.peer_ip = ip;
    tcb.peer_port = port;
    tcb.local_port = (uint16_t)(0xC000 + (isn_base & 0x0FFF));
    tcb.snd_nxt = isn_base;
    isn_base += 40000;
    tcb.rcv_nxt = 0;
    tcb.got_synack = 0;
    tcb.peer_fin = 0;
    tcb.rxlen = 0;
    tcb.state = SYN_SENT;

    int sent = -1;
    for (int t = 0; t < 10 && sent != 0; t++) {
        sent = send_segment(TCP_SYN, NULL, 0); /* fires ARP on a miss */
        if (sent != 0)
            sched_sleep_current(1);
    }
    if (sent != 0) {
        tcb.state = CLOSED;
        return -1;
    }
    uint32_t start = timer_ticks();
    while (!tcb.got_synack && (timer_ticks() - start) < 200)
        sched_sleep_current(1);
    if (!tcb.got_synack) {
        tcb.state = CLOSED;
        return -1;
    }
    tcb.snd_nxt += 1; /* our SYN consumed one sequence number */
    tcb.state = ESTABLISHED;
    send_segment(TCP_ACK, NULL, 0);
    return 0;
}

int tcp_send(const void *data, uint16_t len) {
    if (tcb.state != ESTABLISHED)
        return -1;
    const uint8_t *p = (const uint8_t *)data;
    uint16_t off = 0;
    while (off < len) {
        uint16_t chunk = (uint16_t)(len - off);
        if (chunk > TCP_MAXDATA)
            chunk = TCP_MAXDATA;
        if (send_segment(TCP_PSH | TCP_ACK, p + off, chunk) != 0)
            return (off > 0) ? (int)off : -1;
        tcb.snd_nxt += chunk;
        off += chunk;
    }
    return (int)len;
}

int tcp_recv(uint8_t *buf, uint16_t max) {
    if (tcb.state == CLOSED)
        return -1;
    uint32_t start = timer_ticks();
    while (tcb.rxlen == 0 && !tcb.peer_fin && (timer_ticks() - start) < 300)
        sched_sleep_current(1);
    uint16_t n = tcb.rxlen;
    if (n == 0)
        return tcb.peer_fin ? 0 : -1; /* 0 = peer closed, -1 = timeout */
    if (n > max)
        n = max;
    for (uint16_t i = 0; i < n; i++)
        buf[i] = tcb.rxbuf[i];
    /* Shift any remainder down (single buffer, no ring). */
    uint16_t rem = (uint16_t)(tcb.rxlen - n);
    for (uint16_t i = 0; i < rem; i++)
        tcb.rxbuf[i] = tcb.rxbuf[n + i];
    tcb.rxlen = rem;
    return (int)n;
}

void tcp_close(void) {
    if (tcb.state == ESTABLISHED) {
        send_segment(TCP_FIN | TCP_ACK, NULL, 0);
        tcb.snd_nxt += 1;
        tcb.state = CLOSING;
        uint32_t start = timer_ticks();
        while (!tcb.peer_fin && (timer_ticks() - start) < 100)
            sched_sleep_current(1);
    }
    tcb.state = CLOSED;
}
