/* udp.h - UDP datagrams (IPv4). */
#pragma once
#include <stdint.h>

#include "net.h"

/* Register the UDP handler with the IP layer. */
void udp_init(void);

/* Send a datagram (zero checksum, valid for IPv4 UDP). 0 on success, -1
 * if the next hop isn't resolved. */
int udp_send(ipaddr_t dst, uint16_t src_port, uint16_t dst_port,
             const void *data, uint16_t len);

/* Send a datagram and wait for a reply addressed to src_port, copying up to
 * rxmax bytes into rxbuf. Returns the reply length, or -1 on timeout. One
 * UDP transaction in flight at a time. Syscall context (blocks the task). */
int udp_request(ipaddr_t dst, uint16_t dst_port, uint16_t src_port,
                const void *txdata, uint16_t txlen, uint8_t *rxbuf,
                uint16_t rxmax);

/* Persistently route datagrams arriving on a port to a handler (the UDP
 * payload, IRQ context). Used by services like DHCP. */
typedef void (*udp_listener_fn)(ipaddr_t src, const uint8_t *data,
                                uint16_t len);
void udp_listen(uint16_t port, udp_listener_fn fn);
