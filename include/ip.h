/* ip.h - IPv4: header build/parse, checksum, next-hop routing, demux. */
#pragma once
#include <stdint.h>

#include "net.h"

#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

void ip_init(void);

/* RFC 1071 one's-complement checksum over `len` bytes (network order). */
uint16_t ip_checksum(const void *data, int len);

/* Send an IPv4 packet carrying `payload`. Picks the next hop (the
 * destination if on-subnet, else the gateway) and ARP-resolves it.
 * Returns 0 on success, -1 if the next hop isn't resolved yet. */
int ip_send(ipaddr_t dst, uint8_t protocol, const void *payload, uint16_t len);

/* Send an IPv4 packet with explicit source/destination addresses and a
 * given destination MAC, skipping routing and ARP. For DHCP, which has no
 * address yet and broadcasts (src 0.0.0.0, dst 255.255.255.255, broadcast
 * MAC). Returns 0 on success. */
int ip_send_raw(ipaddr_t src, ipaddr_t dst, const uint8_t dst_mac[6],
                uint8_t protocol, const void *payload, uint16_t len);

/* L4 protocol handler: src is the sender IP; payload/len is the transport
 * segment. Runs in IRQ context. */
typedef void (*ip_proto_fn)(ipaddr_t src, const uint8_t *payload,
                            uint16_t len);
void ip_register(uint8_t protocol, ip_proto_fn fn);

/* Verify ip_checksum against the canonical RFC 1071 example. 1 = ok. */
int ip_selftest(void);
