/* pci.c - PCI configuration-space access and bus-0 enumeration.
 * Uses the legacy 0xCF8/0xCFC config mechanism (#1). QEMU's i440FX exposes
 * all emulated devices on bus 0, including the NIC, so we scan bus 0 and
 * record what we find for later drivers to look up. */
#include <stddef.h>
#include <stdint.h>

#include "io.h"
#include "kprintf.h"
#include "pci.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define MAX_PCI_DEVICES 32

static struct pci_device devs[MAX_PCI_DEVICES];
static int ndev;

static uint32_t config_address(uint8_t bus, uint8_t slot, uint8_t func,
                               uint8_t off) {
    return 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
           ((uint32_t)func << 8) | (off & 0xFC);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func,
                           uint8_t off) {
    outl(PCI_CONFIG_ADDRESS, config_address(bus, slot, func, off));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func,
                           uint8_t off) {
    uint32_t v = pci_config_read32(bus, slot, func, off);
    return (uint16_t)((v >> ((off & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t v = pci_config_read32(bus, slot, func, off);
    return (uint8_t)((v >> ((off & 3) * 8)) & 0xFF);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off,
                        uint32_t val) {
    outl(PCI_CONFIG_ADDRESS, config_address(bus, slot, func, off));
    outl(PCI_CONFIG_DATA, val);
}

static void record(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_config_read16(bus, slot, func, 0x00);
    if (vendor == 0xFFFF || ndev >= MAX_PCI_DEVICES)
        return;
    struct pci_device *d = &devs[ndev++];
    d->bus = bus;
    d->slot = slot;
    d->func = func;
    d->vendor = vendor;
    d->device = pci_config_read16(bus, slot, func, 0x02);
    uint32_t cls = pci_config_read32(bus, slot, func, 0x08);
    d->revision = (uint8_t)cls;
    d->prog_if = (uint8_t)(cls >> 8);
    d->subclass = (uint8_t)(cls >> 16);
    d->class_code = (uint8_t)(cls >> 24);
    d->irq_line = pci_config_read8(bus, slot, func, 0x3C);
    for (int i = 0; i < 6; i++)
        d->bar[i] = pci_config_read32(bus, slot, func, 0x10 + (uint8_t)(i * 4));
    kprintf("pci: %02x:%02x.%x %04x:%04x class %02x:%02x irq %u\n", bus, slot,
            func, d->vendor, d->device, d->class_code, d->subclass,
            d->irq_line);
}

void pci_init(void) {
    ndev = 0;
    for (uint8_t slot = 0; slot < 32; slot++) {
        if (pci_config_read16(0, slot, 0, 0x00) == 0xFFFF)
            continue;
        record(0, slot, 0);
        /* Multifunction devices advertise it in header-type bit 7. */
        if (pci_config_read8(0, slot, 0, 0x0E) & 0x80)
            for (uint8_t func = 1; func < 8; func++)
                if (pci_config_read16(0, slot, func, 0x00) != 0xFFFF)
                    record(0, slot, func);
    }
    kprintf("pci: %d device(s) on bus 0\n", ndev);
}

const struct pci_device *pci_find(uint16_t vendor, uint16_t device) {
    for (int i = 0; i < ndev; i++)
        if (devs[i].vendor == vendor && devs[i].device == device)
            return &devs[i];
    return NULL;
}

const struct pci_device *pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < ndev; i++)
        if (devs[i].class_code == class_code && devs[i].subclass == subclass)
            return &devs[i];
    return NULL;
}

int pci_bar_is_io(uint32_t bar) {
    return bar & 1u;
}
uint32_t pci_bar_io_base(uint32_t bar) {
    return bar & ~0x3u;
}
uint32_t pci_bar_mem_base(uint32_t bar) {
    return bar & ~0xFu;
}

void pci_enable_bus_master(const struct pci_device *d) {
    uint32_t cmd = pci_config_read32(d->bus, d->slot, d->func, 0x04);
    cmd |= 0x7; /* I/O space | memory space | bus master */
    pci_config_write32(d->bus, d->slot, d->func, 0x04, cmd);
}
