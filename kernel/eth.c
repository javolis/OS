/* eth.c - Ethernet framing, transmit, and receive demux.
 *
 * Sits between the RTL8139 driver and the L3 protocols (ARP, IPv4). On RX
 * (IRQ context) it parses the frame header and dispatches to a registered
 * handler by ethertype; on TX it prepends our MAC and the ethertype and
 * hands the frame to the NIC. */
#include <stddef.h>
#include <stdint.h>

#include "kprintf.h"
#include "net.h"
#include "rtl8139.h"

const uint8_t eth_broadcast[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint8_t our_mac[ETH_ALEN];

/* IP config: defaults match QEMU's SLIRP layout; DHCP overrides later. */
static ipaddr_t cfg_ip = IPV4(10, 0, 2, 15);
static ipaddr_t cfg_gw = IPV4(10, 0, 2, 2);
static ipaddr_t cfg_mask = IPV4(255, 255, 255, 0);
static ipaddr_t cfg_dns = IPV4(10, 0, 2, 3);

ipaddr_t net_ip(void) {
    return cfg_ip;
}
ipaddr_t net_gateway(void) {
    return cfg_gw;
}
ipaddr_t net_netmask(void) {
    return cfg_mask;
}
ipaddr_t net_dns(void) {
    return cfg_dns;
}
void net_set_config(ipaddr_t ip, ipaddr_t gw, ipaddr_t mask) {
    cfg_ip = ip;
    cfg_gw = gw;
    cfg_mask = mask;
}
void net_set_dns(ipaddr_t dns) {
    cfg_dns = dns;
}

#define MAX_PROTO 4
static struct {
    uint16_t type;
    eth_proto_fn fn;
} protos[MAX_PROTO];
static int nproto;
static int rx_logged;

static void eth_rx(const uint8_t *frame, uint16_t len) {
    if (len < sizeof(struct eth_header))
        return;
    const struct eth_header *h = (const struct eth_header *)frame;
    uint16_t type = ntohs(h->ethertype);
    if (rx_logged < 16) { /* bounded trace so a frame storm can't spam */
        kprintf("eth: rx ethertype %04x len %u\n", type, len);
        rx_logged++;
    }
    const uint8_t *payload = frame + sizeof(struct eth_header);
    uint16_t plen = (uint16_t)(len - sizeof(struct eth_header));
    for (int i = 0; i < nproto; i++)
        if (protos[i].type == type) {
            protos[i].fn(h->src, payload, plen);
            return;
        }
}

void eth_init(void) {
    rtl8139_get_mac(our_mac);
    rtl8139_set_rx(eth_rx);
}

const uint8_t *net_mac(void) {
    return our_mac;
}

void eth_send(const uint8_t dst[ETH_ALEN], uint16_t ethertype,
              const void *payload, uint16_t len) {
    uint8_t frame[1600];
    if (len > sizeof(frame) - sizeof(struct eth_header))
        return;
    struct eth_header *h = (struct eth_header *)frame;
    for (int i = 0; i < ETH_ALEN; i++) {
        h->dst[i] = dst[i];
        h->src[i] = our_mac[i];
    }
    h->ethertype = htons(ethertype);
    const uint8_t *p = (const uint8_t *)payload;
    for (uint16_t i = 0; i < len; i++)
        frame[sizeof(struct eth_header) + i] = p[i];
    rtl8139_send(frame, (uint16_t)(sizeof(struct eth_header) + len));
}

void net_register(uint16_t ethertype, eth_proto_fn fn) {
    if (nproto < MAX_PROTO) {
        protos[nproto].type = ethertype;
        protos[nproto].fn = fn;
        nproto++;
    }
}
