/* pci.h - minimal PCI configuration-space access and bus enumeration.
 * Enough to find a device (e.g. a NIC), read its BARs and IRQ line, and
 * enable bus mastering. QEMU's i440FX puts everything on bus 0, which is
 * all we scan. */
#pragma once
#include <stdint.h>

/* Configuration-space reads/writes for one device function. The offset is
 * a byte offset into the 256-byte config space; reads are widened from the
 * aligned dword underneath. */
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func,
                           uint8_t off);
uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func,
                           uint8_t off);
uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off,
                        uint32_t val);

struct pci_device {
    uint8_t bus, slot, func;
    uint16_t vendor, device;
    uint8_t class_code, subclass, prog_if, revision;
    uint8_t irq_line; /* config offset 0x3C */
    uint32_t bar[6];  /* raw BAR values */
};

/* Scan the bus and record devices. Call once at boot. */
void pci_init(void);

/* Find the first device matching vendor:device (or class:subclass), or
 * NULL. */
const struct pci_device *pci_find(uint16_t vendor, uint16_t device);
const struct pci_device *pci_find_class(uint8_t class_code, uint8_t subclass);

/* BAR decoding. */
int pci_bar_is_io(uint32_t bar);         /* bit 0 set => I/O space BAR */
uint32_t pci_bar_io_base(uint32_t bar);  /* I/O BAR base address */
uint32_t pci_bar_mem_base(uint32_t bar); /* memory BAR base address */

/* Set I/O space + memory space + bus master in the command register (what a
 * busmastering NIC needs before it can DMA). */
void pci_enable_bus_master(const struct pci_device *d);
