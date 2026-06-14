/* ip.c - IPv4 send/receive: header construction, RFC 1071 checksum,
 * next-hop routing (destination if on-subnet, else the gateway), and demux
 * to transport handlers by protocol number. Receive runs in IRQ context. */
#include <stddef.h>
#include <stdint.h>

#include "arp.h"
#include "ip.h"
#include "kprintf.h"
#include "net.h"

struct ip_header {
    uint8_t version_ihl; /* 0x45 = IPv4, 5-dword header */
    uint8_t tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t src[4];
    uint8_t dst[4];
} __attribute__((packed));

#define MAX_IPPROTO 4
static struct {
    uint8_t proto;
    ip_proto_fn fn;
} protos[MAX_IPPROTO];
static int nproto;
static uint16_t ip_id;
static int rx_logged;

uint16_t ip_checksum(const void *data, int len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    for (int i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)((p[i] << 8) | p[i + 1]); /* big-endian word */
    if (len & 1)
        sum += (uint32_t)(p[len - 1] << 8);
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static void ip_bytes(ipaddr_t ip, uint8_t b[4]) {
    b[0] = (uint8_t)(ip >> 24);
    b[1] = (uint8_t)(ip >> 16);
    b[2] = (uint8_t)(ip >> 8);
    b[3] = (uint8_t)ip;
}

static ipaddr_t ip_from(const uint8_t b[4]) {
    return ((ipaddr_t)b[0] << 24) | ((ipaddr_t)b[1] << 16) |
           ((ipaddr_t)b[2] << 8) | (ipaddr_t)b[3];
}

int ip_send(ipaddr_t dst, uint8_t protocol, const void *payload,
            uint16_t len) {
    uint8_t pkt[1500];
    if ((int)len + (int)sizeof(struct ip_header) > (int)sizeof(pkt))
        return -1;

    /* Next hop: the destination itself if on our subnet, else the gateway. */
    ipaddr_t nexthop = ((dst & net_netmask()) == (net_ip() & net_netmask()))
                           ? dst
                           : net_gateway();
    uint8_t mac[6];
    if (!arp_resolve(nexthop, mac))
        return -1; /* request sent; caller can retry once it's cached */

    struct ip_header *h = (struct ip_header *)pkt;
    h->version_ihl = 0x45;
    h->tos = 0;
    h->total_length = htons((uint16_t)(sizeof(*h) + len));
    h->id = htons(ip_id++);
    h->flags_frag = htons(0x4000); /* don't fragment */
    h->ttl = 64;
    h->protocol = protocol;
    h->checksum = 0;
    ip_bytes(net_ip(), h->src);
    ip_bytes(dst, h->dst);
    h->checksum = htons(ip_checksum(h, sizeof(*h)));

    const uint8_t *pl = (const uint8_t *)payload;
    for (uint16_t i = 0; i < len; i++)
        pkt[sizeof(*h) + i] = pl[i];

    eth_send(mac, ETH_TYPE_IPV4, pkt, (uint16_t)(sizeof(*h) + len));
    return 0;
}

int ip_send_raw(ipaddr_t src, ipaddr_t dst, const uint8_t dst_mac[6],
                uint8_t protocol, const void *payload, uint16_t len) {
    uint8_t pkt[600];
    if ((int)len + (int)sizeof(struct ip_header) > (int)sizeof(pkt))
        return -1;
    struct ip_header *h = (struct ip_header *)pkt;
    h->version_ihl = 0x45;
    h->tos = 0;
    h->total_length = htons((uint16_t)(sizeof(*h) + len));
    h->id = htons(ip_id++);
    h->flags_frag = htons(0x4000);
    h->ttl = 64;
    h->protocol = protocol;
    h->checksum = 0;
    ip_bytes(src, h->src);
    ip_bytes(dst, h->dst);
    h->checksum = htons(ip_checksum(h, sizeof(*h)));
    const uint8_t *pl = (const uint8_t *)payload;
    for (uint16_t i = 0; i < len; i++)
        pkt[sizeof(*h) + i] = pl[i];
    eth_send(dst_mac, ETH_TYPE_IPV4, pkt, (uint16_t)(sizeof(*h) + len));
    return 0;
}

static void ip_rx(const uint8_t *src_mac, const uint8_t *payload,
                  uint16_t len) {
    (void)src_mac;
    if (len < sizeof(struct ip_header))
        return;
    const struct ip_header *h = (const struct ip_header *)payload;
    if ((h->version_ihl >> 4) != 4)
        return;
    uint16_t ihl = (uint16_t)((h->version_ihl & 0x0F) * 4);
    uint16_t total = ntohs(h->total_length);
    if (ihl < sizeof(struct ip_header) || total < ihl || total > len)
        return;
    ipaddr_t src = ip_from(h->src);
    if (rx_logged < 16) {
        kprintf("ip: rx proto %u from %u.%u.%u.%u len %u\n", h->protocol,
                (src >> 24) & 0xFF, (src >> 16) & 0xFF, (src >> 8) & 0xFF,
                src & 0xFF, total);
        rx_logged++;
    }
    for (int i = 0; i < nproto; i++)
        if (protos[i].proto == h->protocol) {
            protos[i].fn(src, payload + ihl, (uint16_t)(total - ihl));
            return;
        }
}

void ip_register(uint8_t protocol, ip_proto_fn fn) {
    if (nproto < MAX_IPPROTO) {
        protos[nproto].proto = protocol;
        protos[nproto].fn = fn;
        nproto++;
    }
}

void ip_init(void) {
    net_register(ETH_TYPE_IPV4, ip_rx);
}

int ip_selftest(void) {
    /* Canonical RFC 1071 / Wikipedia example header; checksum field zeroed,
     * the correct checksum is 0xB861, and the sum over the complete header
     * (with checksum present) is zero. */
    static const uint8_t hdr0[20] = {0x45, 0x00, 0x00, 0x73, 0x00, 0x00, 0x40,
                                      0x00, 0x40, 0x11, 0x00, 0x00, 0xc0, 0xa8,
                                      0x00, 0x01, 0xc0, 0xa8, 0x00, 0xc7};
    static const uint8_t hdr1[20] = {0x45, 0x00, 0x00, 0x73, 0x00, 0x00, 0x40,
                                      0x00, 0x40, 0x11, 0xb8, 0x61, 0xc0, 0xa8,
                                      0x00, 0x01, 0xc0, 0xa8, 0x00, 0xc7};
    int ok = (ip_checksum(hdr0, 20) == 0xB861) && (ip_checksum(hdr1, 20) == 0);
    kprintf("ip: checksum self-test %s\n", ok ? "ok" : "FAILED");
    return ok;
}
