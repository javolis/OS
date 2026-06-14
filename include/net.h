/* net.h - Ethernet layer and shared networking helpers. */
#pragma once
#include <stdint.h>

#define ETH_ALEN 6
#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV4 0x0800

/* IPv4 addresses are kept in host byte order; IPV4(a,b,c,d) builds one. */
typedef uint32_t ipaddr_t;
#define IPV4(a, b, c, d)                                                       \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) |    \
     (uint32_t)(d))

/* Host <-> network byte order (this kernel is little-endian x86). */
static inline uint16_t htons(uint16_t x) {
    return (uint16_t)((x << 8) | (x >> 8));
}
static inline uint16_t ntohs(uint16_t x) {
    return htons(x);
}
static inline uint32_t htonl(uint32_t x) {
    return ((x & 0xFFu) << 24) | ((x & 0xFF00u) << 8) | ((x >> 8) & 0xFF00u) |
           ((x >> 24) & 0xFFu);
}
static inline uint32_t ntohl(uint32_t x) {
    return htonl(x);
}

struct eth_header {
    uint8_t dst[ETH_ALEN];
    uint8_t src[ETH_ALEN];
    uint16_t ethertype; /* network byte order */
} __attribute__((packed));

extern const uint8_t eth_broadcast[ETH_ALEN];

/* Bring up the Ethernet layer on the NIC (installs the RX callback). */
void eth_init(void);

/* Our 6-byte MAC address. */
const uint8_t *net_mac(void);

/* Send one frame: ethertype is host order; payload/len is the L3 payload. */
void eth_send(const uint8_t dst[ETH_ALEN], uint16_t ethertype,
              const void *payload, uint16_t len);

/* A handler for one ethertype (host order). Runs in IRQ context; payload
 * points into the RX ring and is only valid during the call. */
typedef void (*eth_proto_fn)(const uint8_t *src_mac, const uint8_t *payload,
                             uint16_t len);
void net_register(uint16_t ethertype, eth_proto_fn fn);

/* IP configuration (defaults to the static SLIRP layout; DHCP overrides). */
ipaddr_t net_ip(void);
ipaddr_t net_gateway(void);
ipaddr_t net_netmask(void);
ipaddr_t net_dns(void);
void net_set_config(ipaddr_t ip, ipaddr_t gw, ipaddr_t mask);
void net_set_dns(ipaddr_t dns);
