/* tcp.h - minimal TCP client (one connection at a time). */
#pragma once
#include <stdint.h>

#include "net.h"

/* Register the TCP handler with the IP layer. */
void tcp_init(void);

/* Open a connection to ip:port (handshake). Returns 0 on success, -1 on
 * failure/timeout. Syscall context (blocks the task). */
int tcp_connect(ipaddr_t ip, uint16_t port);

/* Send data on the open connection. Returns bytes queued, or -1. */
int tcp_send(const void *data, uint16_t len);

/* Receive up to max bytes. Returns the byte count, 0 if the peer closed
 * (FIN) with nothing pending, or -1 on error/timeout. Syscall context. */
int tcp_recv(uint8_t *buf, uint16_t max);

/* Close the connection (sends FIN). */
void tcp_close(void);
