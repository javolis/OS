/* fbtest.c - userland: exercise SYS_FBINFO and the /dev/fb device.
 *
 * Queries the framebuffer geometry, then writes a known byte pattern to
 * /dev/fb and reads it back through a fresh fd (each open starts at offset
 * 0), proving the device moves raw pixel bytes both directions. CI cannot
 * see the screen, so this self-checking round-trip is the proof. */
#include "ulib.h"

void _start(void) {
    struct fbinfo fi;
    if (sys_fbinfo(&fi) != 0) {
        /* No framebuffer (VGA-text-only boot): nothing to test, not a fail. */
        uprintf("fbtest: no framebuffer\n");
        sys_exit(0);
    }

    /* Geometry sanity: non-empty, a supported depth, pitch covers a row. */
    int bpp_ok = (fi.bpp == 24 || fi.bpp == 32);
    if (fi.width == 0 || fi.height == 0 || !bpp_ok ||
        fi.pitch < fi.width * (fi.bpp / 8)) {
        uprintf("fbtest: FAIL geometry %ux%u pitch=%u bpp=%u\n", fi.width,
                fi.height, fi.pitch, fi.bpp);
        sys_exit(1);
    }

    /* /dev/fb opens only when a framebuffer exists. */
    int fd = sys_open("/dev/fb");
    if (fd < 0) {
        uprintf("fbtest: FAIL open /dev/fb\n");
        sys_exit(1);
    }

    unsigned char pat[16];
    for (int i = 0; i < 16; i++)
        pat[i] = (unsigned char)(0x10 + i * 7); /* distinct, not all-equal */

    int w = sys_writefd(fd, (const char *)pat, 16);
    sys_close(fd);
    if (w != 16) {
        uprintf("fbtest: FAIL write w=%d\n", w);
        sys_exit(1);
    }

    /* Fresh fd -> offset back at 0 -> read the same bytes back. */
    fd = sys_open("/dev/fb");
    unsigned char back[16];
    for (int i = 0; i < 16; i++)
        back[i] = 0xAA; /* poison */
    int r = sys_read(fd, (char *)back, 16);
    sys_close(fd);

    int match = (r == 16);
    for (int i = 0; i < 16; i++)
        if (back[i] != pat[i])
            match = 0;
    if (!match) {
        uprintf("fbtest: FAIL round-trip r=%d\n", r);
        sys_exit(1);
    }

    uprintf("fbtest: dev/fb round-trip ok %ux%u %ubpp pitch=%u\n", fi.width,
            fi.height, fi.bpp, fi.pitch);
    sys_exit(0);
}
