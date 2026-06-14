/* dhcp.c - minimal DHCP client.
 *
 * DHCP runs before we have an address, so it can't use the normal IP/ARP
 * path: requests are broadcast (src 0.0.0.0 -> 255.255.255.255, broadcast
 * MAC) via ip_send_raw, and replies arrive on UDP port 68 via a listener.
 * dhcp_run drives DISCOVER/OFFER/REQUEST/ACK, sleeping between steps, and
 * applies the lease. The broadcast flag is set so the server broadcasts its
 * replies (we have no address to unicast to yet). */
#include <stddef.h>
#include <stdint.h>

#include "dhcp.h"
#include "ip.h"
#include "kprintf.h"
#include "net.h"
#include "sched.h"
#include "timer.h"
#include "udp.h"

#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5

#define DHCP_XID 0x21436587u

static volatile int got_offer, got_ack;
static ipaddr_t off_ip, off_mask, off_gw, off_dns, off_server;

static ipaddr_t rd32(const uint8_t *p) {
    return ((ipaddr_t)p[0] << 24) | ((ipaddr_t)p[1] << 16) |
           ((ipaddr_t)p[2] << 8) | (ipaddr_t)p[3];
}

static void wr32(uint8_t *p, ipaddr_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* Build a BOOTP/DHCP message into buf (>= 300 bytes). Returns its length. */
static int build_dhcp(uint8_t *buf, uint8_t msgtype, ipaddr_t req_ip,
                      ipaddr_t server) {
    for (int i = 0; i < 300; i++)
        buf[i] = 0;
    buf[0] = 1; /* op = BOOTREQUEST */
    buf[1] = 1; /* htype = Ethernet */
    buf[2] = 6; /* hlen */
    wr32(buf + 4, DHCP_XID);
    buf[10] = 0x80; /* flags: broadcast */
    const uint8_t *mac = net_mac();
    for (int i = 0; i < 6; i++)
        buf[28 + i] = mac[i]; /* chaddr */
    buf[236] = 0x63;          /* magic cookie */
    buf[237] = 0x82;
    buf[238] = 0x53;
    buf[239] = 0x63;
    int o = 240;
    buf[o++] = 53; /* option: DHCP message type */
    buf[o++] = 1;
    buf[o++] = msgtype;
    if (msgtype == DHCP_REQUEST) {
        buf[o++] = 50; /* requested IP */
        buf[o++] = 4;
        wr32(buf + o, req_ip);
        o += 4;
        buf[o++] = 54; /* server identifier */
        buf[o++] = 4;
        wr32(buf + o, server);
        o += 4;
    }
    buf[o++] = 55; /* parameter request list: mask, router, DNS */
    buf[o++] = 3;
    buf[o++] = 1;
    buf[o++] = 3;
    buf[o++] = 6;
    buf[o++] = 255; /* end */
    return o;
}

static void send_dhcp(uint8_t msgtype, ipaddr_t req_ip, ipaddr_t server) {
    uint8_t dhcp[300];
    int dlen = build_dhcp(dhcp, msgtype, req_ip, server);
    uint8_t udp[8 + 300];
    uint16_t ulen = (uint16_t)(8 + dlen);
    udp[0] = 0; /* sport 68 */
    udp[1] = 68;
    udp[2] = 0; /* dport 67 */
    udp[3] = 67;
    udp[4] = (uint8_t)(ulen >> 8);
    udp[5] = (uint8_t)ulen;
    udp[6] = 0; /* checksum 0 */
    udp[7] = 0;
    for (int i = 0; i < dlen; i++)
        udp[8 + i] = dhcp[i];
    ip_send_raw(0, 0xFFFFFFFFu, eth_broadcast, IPPROTO_UDP, udp, ulen);
}

static void dhcp_udp_rx(ipaddr_t src, const uint8_t *m, uint16_t len) {
    (void)src;
    if (len < 240 || m[0] != 2 || rd32(m + 4) != DHCP_XID)
        return;
    if (m[236] != 0x63 || m[237] != 0x82 || m[238] != 0x53 || m[239] != 0x63)
        return;
    ipaddr_t yi = rd32(m + 16);
    uint8_t msgtype = 0;
    ipaddr_t mask = 0, router = 0, dns = 0, server = 0;
    int o = 240;
    while (o < (int)len && m[o] != 255) {
        if (m[o] == 0) {
            o++;
            continue;
        }
        uint8_t code = m[o++];
        if (o >= (int)len)
            break;
        uint8_t l = m[o++];
        if (o + l > (int)len)
            break;
        const uint8_t *v = m + o;
        if (code == 53 && l >= 1)
            msgtype = v[0];
        else if (code == 1 && l >= 4)
            mask = rd32(v);
        else if (code == 3 && l >= 4)
            router = rd32(v);
        else if (code == 6 && l >= 4)
            dns = rd32(v);
        else if (code == 54 && l >= 4)
            server = rd32(v);
        o += l;
    }
    if (msgtype == DHCP_OFFER) {
        off_ip = yi;
        off_mask = mask;
        off_gw = router;
        off_dns = dns;
        off_server = server;
        got_offer = 1;
    } else if (msgtype == DHCP_ACK) {
        off_ip = yi;
        if (mask)
            off_mask = mask;
        if (router)
            off_gw = router;
        if (dns)
            off_dns = dns;
        got_ack = 1;
    }
}

void dhcp_init(void) {
    udp_listen(68, dhcp_udp_rx);
}

ipaddr_t dhcp_run(void) {
    got_offer = 0;
    got_ack = 0;

    send_dhcp(DHCP_DISCOVER, 0, 0);
    uint32_t start = timer_ticks();
    while (!got_offer && (timer_ticks() - start) < 200)
        sched_sleep_current(1);
    if (!got_offer)
        return 0;

    send_dhcp(DHCP_REQUEST, off_ip, off_server);
    start = timer_ticks();
    while (!got_ack && (timer_ticks() - start) < 200)
        sched_sleep_current(1);
    if (!got_ack)
        return 0;

    ipaddr_t gw = off_gw ? off_gw : net_gateway();
    ipaddr_t mask = off_mask ? off_mask : net_netmask();
    net_set_config(off_ip, gw, mask);
    if (off_dns)
        net_set_dns(off_dns);

    kprintf("dhcp: bound %u.%u.%u.%u gw %u.%u.%u.%u dns %u.%u.%u.%u\n",
            (off_ip >> 24) & 0xFF, (off_ip >> 16) & 0xFF, (off_ip >> 8) & 0xFF,
            off_ip & 0xFF, (gw >> 24) & 0xFF, (gw >> 16) & 0xFF, (gw >> 8) & 0xFF,
            gw & 0xFF, (off_dns >> 24) & 0xFF, (off_dns >> 16) & 0xFF,
            (off_dns >> 8) & 0xFF, off_dns & 0xFF);
    return off_ip;
}
