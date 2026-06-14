/* icmp.h - ICMP echo (ping). */
#pragma once
#include <stdint.h>

#include "net.h"

/* Register the ICMP handler with the IP layer. */
void icmp_init(void);

/* Send an echo request to dst and wait for the reply. Returns the
 * round-trip time in timer ticks (>= 0), or -1 on timeout. Call from a
 * syscall context: it blocks the task (sleeping in tick slices) while the
 * reply arrives via the RX IRQ. One ping in flight system-wide at a time. */
int icmp_ping(ipaddr_t dst);
