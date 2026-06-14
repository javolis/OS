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

void eth_arp_probe(void) {
    /* A minimal ARP request (who-has 10.0.2.2, tell 10.0.2.15) so SLIRP
     * replies and we can confirm the RX path. The real ARP module supplants
     * this next. */
    uint8_t arp[28];
    arp[0] = 0x00;
    arp[1] = 0x01; /* htype = Ethernet */
    arp[2] = 0x08;
    arp[3] = 0x00; /* ptype = IPv4 */
    arp[4] = ETH_ALEN;
    arp[5] = 4; /* hlen / plen */
    arp[6] = 0x00;
    arp[7] = 0x01; /* oper = request */
    for (int i = 0; i < ETH_ALEN; i++)
        arp[8 + i] = our_mac[i]; /* sender hardware addr */
    arp[14] = 10;
    arp[15] = 0;
    arp[16] = 2;
    arp[17] = 15; /* sender protocol addr 10.0.2.15 */
    for (int i = 0; i < ETH_ALEN; i++)
        arp[18 + i] = 0; /* target hardware addr (unknown) */
    arp[24] = 10;
    arp[25] = 0;
    arp[26] = 2;
    arp[27] = 2; /* target protocol addr 10.0.2.2 */
    eth_send(eth_broadcast, ETH_TYPE_ARP, arp, sizeof(arp));
}
