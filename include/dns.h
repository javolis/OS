/* dns.h - tiny DNS A-record resolver over UDP. */
#pragma once
#include <stdint.h>

#include "net.h"

/* Resolve a hostname to an IPv4 address via the configured DNS server.
 * Returns 1 and fills *out on success, 0 otherwise. Syscall context. */
int dns_resolve(const char *name, ipaddr_t *out);
