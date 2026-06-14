/* rtl8139.h - RealTek RTL8139 NIC driver. */
#pragma once
#include <stdint.h>

/* Called from IRQ context for each received Ethernet frame, with the
 * trailing CRC already stripped. Must not block. */
typedef void (*rtl8139_rx_cb)(const uint8_t *frame, uint16_t len);

/* Find the PCI NIC and bring it up. Returns 0 on success, -1 if absent. */
int rtl8139_init(void);

int rtl8139_present(void);
void rtl8139_get_mac(uint8_t mac[6]);

/* Transmit one raw Ethernet frame (runt frames are padded to 60 bytes).
 * Returns the byte count queued, or -1. */
int rtl8139_send(const void *frame, uint16_t len);

/* Register the receive callback (the Ethernet layer installs it). */
void rtl8139_set_rx(rtl8139_rx_cb cb);
