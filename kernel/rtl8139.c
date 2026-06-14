/* rtl8139.c - RealTek RTL8139 NIC driver (PIO config + busmaster DMA).
 *
 * The NIC DMAs to/from physical addresses, so the RX ring and TX buffers
 * are static kernel arrays: the kernel image is loaded contiguously, so a
 * static buffer is physically contiguous and virt_to_phys gives the address
 * the NIC needs. Receive runs in IRQ context and hands each frame to a
 * registered callback (the Ethernet layer). */
#include <stddef.h>
#include <stdint.h>

#include "io.h"
#include "irq.h"
#include "kprintf.h"
#include "memlayout.h"
#include "pci.h"
#include "pic.h"
#include "rtl8139.h"

#define RTL_VENDOR 0x10EC
#define RTL_DEVICE 0x8139

/* Register offsets from the I/O BAR. */
#define REG_MAC0 0x00
#define REG_TSD0 0x10    /* transmit status/command, 4 x dword */
#define REG_TSAD0 0x20   /* transmit start address, 4 x dword */
#define REG_RBSTART 0x30 /* RX buffer physical address */
#define REG_CMD 0x37
#define REG_CAPR 0x38 /* current address of packet read */
#define REG_IMR 0x3C  /* interrupt mask */
#define REG_ISR 0x3E  /* interrupt status */
#define REG_RCR 0x44  /* receive config */
#define REG_CONFIG1 0x52

#define CMD_RST 0x10
#define CMD_RE 0x08
#define CMD_TE 0x04
#define CMD_BUFE 0x01 /* RX buffer empty */

#define ISR_ROK 0x0001
#define ISR_TOK 0x0004

/* 8 KiB ring + 16-byte header slack + room for one frame past the end (the
 * WRAP bit lets a packet overrun rather than split at the boundary). */
#define RX_BUF_SIZE (8192 + 16 + 1536)
#define TX_SLOTS 4
#define TX_BUF_SIZE 1792

static uint8_t rx_buffer[RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffer[TX_SLOTS][TX_BUF_SIZE] __attribute__((aligned(16)));

static uint16_t iobase;
static uint8_t mac[6];
static int present;
static int rx_offset;
static int tx_cur;
static rtl8139_rx_cb rx_cb;

static void copy(uint8_t *dst, const uint8_t *src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        dst[i] = src[i];
}

static void rtl8139_irq(struct registers *regs) {
    (void)regs;
    uint16_t status = inw(iobase + REG_ISR);
    outw(iobase + REG_ISR, status); /* ack (write-1-to-clear) */
    if (!(status & ISR_ROK))
        return;
    /* Drain every packet the NIC has written since we last looked. */
    while (!(inb(iobase + REG_CMD) & CMD_BUFE)) {
        const uint16_t *hdr = (const uint16_t *)(rx_buffer + rx_offset);
        uint16_t pkt_status = hdr[0];
        uint16_t pkt_len = hdr[1]; /* includes the 4-byte CRC */
        if ((pkt_status & 0x01) && pkt_len >= 4 && rx_cb)
            rx_cb(rx_buffer + rx_offset + 4, (uint16_t)(pkt_len - 4));
        rx_offset = (rx_offset + pkt_len + 4 + 3) & ~3; /* next dword-aligned */
        if (rx_offset > 8192)
            rx_offset -= 8192;
        outw(iobase + REG_CAPR, (uint16_t)(rx_offset - 0x10));
    }
}

int rtl8139_init(void) {
    const struct pci_device *dev = pci_find(RTL_VENDOR, RTL_DEVICE);
    if (!dev) {
        kprintf("rtl8139: no NIC found\n");
        return -1;
    }
    pci_enable_bus_master(dev);
    iobase = (uint16_t)pci_bar_io_base(dev->bar[0]);

    outb(iobase + REG_CONFIG1, 0x00);          /* power on */
    outb(iobase + REG_CMD, CMD_RST);           /* software reset */
    while (inb(iobase + REG_CMD) & CMD_RST) {} /* wait for it to clear */

    outl(iobase + REG_RBSTART, virt_to_phys(rx_buffer));
    outw(iobase + REG_IMR, ISR_ROK | ISR_TOK);
    /* accept broadcast | multicast | physical-match | all + WRAP, 8K ring */
    outl(iobase + REG_RCR, 0x0F | (1u << 7));
    outb(iobase + REG_CMD, CMD_RE | CMD_TE);

    for (int i = 0; i < 6; i++)
        mac[i] = inb(iobase + REG_MAC0 + (uint16_t)i);

    rx_offset = 0;
    tx_cur = 0;
    present = 1;

    irq_register(dev->irq_line, rtl8139_irq);
    pic_unmask(dev->irq_line);

    kprintf("rtl8139: MAC %02x:%02x:%02x:%02x:%02x:%02x irq %u iobase %04x\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], dev->irq_line,
            iobase);
    return 0;
}

int rtl8139_present(void) {
    return present;
}

void rtl8139_get_mac(uint8_t out[6]) {
    for (int i = 0; i < 6; i++)
        out[i] = mac[i];
}

void rtl8139_set_rx(rtl8139_rx_cb cb) {
    rx_cb = cb;
}

int rtl8139_send(const void *frame, uint16_t len) {
    if (!present || len > TX_BUF_SIZE)
        return -1;
    int slot = tx_cur;
    copy(tx_buffer[slot], frame, len);
    uint16_t tlen = len;
    if (tlen < 60) { /* pad runts to the 60-byte Ethernet minimum */
        for (uint16_t i = len; i < 60; i++)
            tx_buffer[slot][i] = 0;
        tlen = 60;
    }
    outl(iobase + REG_TSAD0 + (uint16_t)(slot * 4), virt_to_phys(tx_buffer[slot]));
    outl(iobase + REG_TSD0 + (uint16_t)(slot * 4), tlen); /* OWN=0 starts TX */
    tx_cur = (tx_cur + 1) & (TX_SLOTS - 1);
    return len;
}
