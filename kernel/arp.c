/* arp.c - Address Resolution Protocol: a small IP->MAC cache, replies to
 * requests for our address, and learns from every ARP packet seen. Runs in
 * IRQ context on receive. */
#include <stddef.h>
#include <stdint.h>

#include "arp.h"
#include "kprintf.h"
#include "net.h"

struct arp_packet {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6]; /* sender hardware addr */
    uint8_t spa[4]; /* sender protocol addr */
    uint8_t tha[6]; /* target hardware addr */
    uint8_t tpa[4]; /* target protocol addr */
} __attribute__((packed));

#define ARP_CACHE_N 16
static struct {
    ipaddr_t ip;
    uint8_t mac[6];
    int valid;
} cache[ARP_CACHE_N];
static int arp_logged;

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

static void cache_put(ipaddr_t ip, const uint8_t mac[6]) {
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_N; i++) {
        if (cache[i].valid && cache[i].ip == ip) {
            slot = i;
            break;
        }
        if (slot < 0 && !cache[i].valid)
            slot = i;
    }
    if (slot < 0)
        slot = 0; /* table full: recycle the first slot */
    cache[slot].ip = ip;
    cache[slot].valid = 1;
    for (int i = 0; i < 6; i++)
        cache[slot].mac[i] = mac[i];
    if (arp_logged < 8) {
        kprintf("arp: %u.%u.%u.%u is %02x:%02x:%02x:%02x:%02x:%02x\n",
                (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF,
                ip & 0xFF, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        arp_logged++;
    }
}

int arp_lookup(ipaddr_t ip, uint8_t mac[6]) {
    for (int i = 0; i < ARP_CACHE_N; i++)
        if (cache[i].valid && cache[i].ip == ip) {
            for (int j = 0; j < 6; j++)
                mac[j] = cache[i].mac[j];
            return 1;
        }
    return 0;
}

static void arp_send(uint16_t oper, const uint8_t target_mac[6],
                     ipaddr_t target_ip) {
    struct arp_packet p;
    p.htype = htons(1);
    p.ptype = htons(0x0800);
    p.hlen = 6;
    p.plen = 4;
    p.oper = htons(oper);
    const uint8_t *mymac = net_mac();
    for (int i = 0; i < 6; i++) {
        p.sha[i] = mymac[i];
        p.tha[i] = target_mac[i];
    }
    ip_bytes(net_ip(), p.spa);
    ip_bytes(target_ip, p.tpa);
    const uint8_t *dst = (oper == 1) ? eth_broadcast : target_mac;
    eth_send(dst, ETH_TYPE_ARP, &p, sizeof(p));
}

void arp_request(ipaddr_t ip) {
    const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};
    arp_send(1, zero, ip);
}

static void arp_rx(const uint8_t *src_mac, const uint8_t *payload,
                   uint16_t len) {
    (void)src_mac;
    if (len < sizeof(struct arp_packet))
        return;
    const struct arp_packet *p = (const struct arp_packet *)payload;
    if (ntohs(p->ptype) != 0x0800 || p->plen != 4)
        return;
    ipaddr_t spa = ip_from(p->spa);
    cache_put(spa, p->sha); /* learn the sender either way */
    if (ntohs(p->oper) == 1) { /* a request: answer if it targets us */
        ipaddr_t tpa = ip_from(p->tpa);
        if (tpa == net_ip())
            arp_send(2, p->sha, spa);
    }
}

void arp_init(void) {
    net_register(ETH_TYPE_ARP, arp_rx);
}

int arp_resolve(ipaddr_t ip, uint8_t mac[6]) {
    if (arp_lookup(ip, mac))
        return 1;
    arp_request(ip);
    return 0;
}
