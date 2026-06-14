/* dhcp.h - minimal DHCP client (obtain an IPv4 lease). */
#pragma once
#include <stdint.h>

#include "net.h"

/* Register the DHCP reply listener (UDP port 68). */
void dhcp_init(void);

/* Run the DISCOVER/OFFER/REQUEST/ACK handshake and apply the lease via
 * net_set_config/net_set_dns. Returns the leased IPv4 address (host order),
 * or 0 on failure. Syscall context (blocks the task). */
ipaddr_t dhcp_run(void);
