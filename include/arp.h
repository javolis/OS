/* arp.h - Address Resolution Protocol (IPv4 over Ethernet). */
#pragma once
#include <stdint.h>

#include "net.h"

/* Register the ARP handler with the Ethernet layer. */
void arp_init(void);

/* Broadcast a request asking who owns `ip`. */
void arp_request(ipaddr_t ip);

/* Look up a cached IP->MAC mapping. Returns 1 and fills mac if known. */
int arp_lookup(ipaddr_t ip, uint8_t mac[6]);

/* Resolve `ip` to a MAC. Returns 1 immediately if cached; otherwise sends
 * a request and returns 0 (the reply caches asynchronously via the RX IRQ,
 * so a later call hits). The gateway is pre-resolved at boot. */
int arp_resolve(ipaddr_t ip, uint8_t mac[6]);
