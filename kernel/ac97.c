/* ac97.c — Intel AC'97 audio controller (ICH / QEMU "AC97") PCM output.
 *
 * AC'97 splits its registers across two I/O BARs: BAR0 is the mixer (NAM,
 * volumes + codec reset) and BAR1 is the bus-master engine (NABM, the DMA
 * scatter-gather playback). Output works off a Buffer Descriptor List: an
 * array of {physical address, length, flags} entries the controller walks,
 * DMAing each buffer to the codec at the sample rate (48 kHz here).
 *
 * We do one-shot, polled playback: copy the caller's PCM into a static DMA
 * buffer (physically contiguous, like the NIC rings), build a single-entry
 * BDL, point the controller at it and set the run bit. The caller polls
 * ac97_busy() until the DMA halts. No IRQ handler is needed. */
#include <stddef.h>
#include <stdint.h>

#include "ac97.h"
#include "io.h"
#include "kprintf.h"
#include "memlayout.h"
#include "pci.h"

#define AC97_VENDOR 0x8086
#define AC97_DEVICE 0x2415 /* 82801AA AC'97 audio (what QEMU's -device AC97 is) */

/* Mixer (NAM, BAR0) register offsets. */
#define NAM_RESET 0x00
#define NAM_MASTER_VOL 0x02
#define NAM_PCM_OUT_VOL 0x18
#define NAM_REC_SELECT 0x1A /* input source select (0 = mic) */
#define NAM_REC_GAIN 0x1C   /* record gain */

/* Bus master (NABM, BAR1) register offsets. The PCM-in box is at 0x00, the
 * PCM-out box at 0x10; both share the same layout. */
#define NABM_PI_BDBAR 0x00
#define NABM_PI_LVI 0x05
#define NABM_PI_SR 0x06
#define NABM_PI_CR 0x0B
#define NABM_PO_BDBAR 0x10 /* dword: BDL physical base */
#define NABM_PO_CIV 0x14   /* byte: current index (RO) */
#define NABM_PO_LVI 0x15   /* byte: last valid index */
#define NABM_PO_SR 0x16    /* word: status */
#define NABM_PO_PICB 0x18  /* word: position in current buffer (RO) */
#define NABM_PO_CR 0x1B    /* byte: control */
#define NABM_GLOB_CNT 0x2C /* dword: global control */

/* PO_CR control bits. */
#define CR_RPBM 0x01 /* run bus master (play/pause) */
#define CR_RR 0x02   /* reset registers (only when halted) */

/* PO_SR status bits. */
#define SR_DCH 0x01 /* DMA controller halted */

/* BDL entry flag bits (high word of the second dword). */
#define BDL_IOC 0x8000 /* interrupt on completion */
#define BDL_BUP 0x4000 /* buffer underrun policy: feed silence when drained */

#define AC97_BDL_ENTRIES 32
#define AC97_MAX_SAMPLES 48000u /* 0.5 s of 16-bit stereo */

struct bdl_entry {
    uint32_t addr;     /* buffer physical address */
    uint16_t samples;  /* number of 16-bit samples in the buffer */
    uint16_t flags;    /* IOC / BUP */
} __attribute__((packed));

static struct bdl_entry bdl[AC97_BDL_ENTRIES] __attribute__((aligned(8)));
static struct bdl_entry cap_bdl[AC97_BDL_ENTRIES] __attribute__((aligned(8)));
static int16_t dma_buf[AC97_MAX_SAMPLES] __attribute__((aligned(4)));
static int16_t cap_buf[AC97_MAX_SAMPLES] __attribute__((aligned(4)));

static uint16_t nam, nabm;
static int present;
static int cur_vol = 100; /* master volume percent */

int ac97_init(void) {
    const struct pci_device *dev = pci_find(AC97_VENDOR, AC97_DEVICE);
    if (!dev) {
        kprintf("ac97: no audio controller found\n");
        return -1;
    }
    pci_enable_bus_master(dev);
    nam = (uint16_t)pci_bar_io_base(dev->bar[0]);
    nabm = (uint16_t)pci_bar_io_base(dev->bar[1]);

    /* Bring the codec out of cold reset. */
    outl(nabm + NABM_GLOB_CNT, 0x00000002);
    for (volatile int i = 0; i < 200000; i++) {
    }

    outw(nam + NAM_RESET, 0x0001);      /* reset the mixer */
    outw(nam + NAM_MASTER_VOL, 0x0000); /* 0 dB attenuation (full volume) */
    outw(nam + NAM_PCM_OUT_VOL, 0x0000);
    outw(nam + NAM_REC_SELECT, 0x0000); /* capture source: mic, both channels */
    outw(nam + NAM_REC_GAIN, 0x0000);   /* 0 dB record gain */

    /* Reset the PCM-out DMA engine so CIV/LVI/SR start clean. */
    outb(nabm + NABM_PO_CR, CR_RR);
    for (volatile int i = 0; i < 200000 && (inb(nabm + NABM_PO_CR) & CR_RR);
         i++) {
    }

    present = 1;
    kprintf("ac97: ready (nam %04x nabm %04x irq %u)\n", nam, nabm,
            dev->irq_line);
    return 0;
}

int ac97_present(void) {
    return present;
}

uint32_t ac97_capacity(void) {
    return AC97_MAX_SAMPLES;
}

int ac97_play(const int16_t *samples, uint32_t count) {
    if (!present)
        return -1;
    if (count > AC97_MAX_SAMPLES)
        count = AC97_MAX_SAMPLES;
    for (uint32_t i = 0; i < count; i++)
        dma_buf[i] = samples[i];

    /* Halt + reset the engine before reprogramming the BDL. */
    outb(nabm + NABM_PO_CR, CR_RR);
    for (volatile int i = 0; i < 200000 && (inb(nabm + NABM_PO_CR) & CR_RR);
         i++) {
    }

    bdl[0].addr = virt_to_phys(dma_buf);
    bdl[0].samples = (uint16_t)count;
    bdl[0].flags = BDL_IOC | BDL_BUP;

    outl(nabm + NABM_PO_BDBAR, virt_to_phys(bdl));
    outb(nabm + NABM_PO_LVI, 0); /* only entry 0 is valid */
    outb(nabm + NABM_PO_CR, CR_RPBM); /* run */
    return (int)count;
}

int ac97_busy(void) {
    if (!present)
        return 0;
    return !(inw(nabm + NABM_PO_SR) & SR_DCH);
}

void ac97_stop(void) {
    if (!present)
        return;
    outb(nabm + NABM_PO_CR, 0x00);
}

int ac97_capture_start(uint32_t count) {
    if (!present)
        return -1;
    if (count > AC97_MAX_SAMPLES)
        count = AC97_MAX_SAMPLES;

    /* Halt + reset the capture engine before reprogramming its BDL. */
    outb(nabm + NABM_PI_CR, CR_RR);
    for (volatile int i = 0; i < 200000 && (inb(nabm + NABM_PI_CR) & CR_RR);
         i++) {
    }

    cap_bdl[0].addr = virt_to_phys(cap_buf);
    cap_bdl[0].samples = (uint16_t)count;
    cap_bdl[0].flags = BDL_IOC | BDL_BUP;

    outl(nabm + NABM_PI_BDBAR, virt_to_phys(cap_bdl));
    outb(nabm + NABM_PI_LVI, 0);
    outb(nabm + NABM_PI_CR, CR_RPBM); /* run capture */
    return (int)count;
}

int ac97_capture_busy(void) {
    if (!present)
        return 0;
    return !(inw(nabm + NABM_PI_SR) & SR_DCH);
}

void ac97_capture_stop(void) {
    if (!present)
        return;
    outb(nabm + NABM_PI_CR, 0x00);
}

void ac97_capture_read(int16_t *out, uint32_t count) {
    if (count > AC97_MAX_SAMPLES)
        count = AC97_MAX_SAMPLES;
    for (uint32_t i = 0; i < count; i++)
        out[i] = cap_buf[i];
}

/* AC'97 volume registers store attenuation (0 = loudest, higher = quieter,
 * bit 15 = mute), so map a 0-100% level inversely. */
int ac97_set_volume(int pct) {
    if (!present)
        return -1;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    cur_vol = pct;
    if (pct == 0) {
        outw(nam + NAM_MASTER_VOL, 0x8000); /* mute */
        outw(nam + NAM_PCM_OUT_VOL, 0x8000);
    } else {
        int att = (100 - pct) * 0x3F / 100;
        uint16_t v = (uint16_t)((att << 8) | att);
        outw(nam + NAM_MASTER_VOL, v);
        outw(nam + NAM_PCM_OUT_VOL, v);
    }
    return pct;
}

int ac97_volume(void) {
    return present ? cur_vol : -1;
}
