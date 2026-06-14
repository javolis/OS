/* gfxcap.c - graphics capstone: one program exercising the whole graphics
 * stack (SYS_FBINFO, /dev/fb, every ugfx primitive, clipping, text, flush)
 * and tallying the result, like runtests.elf for the syscall surface.
 * Prints "gfxcap: ALL PASS (N tests)" only if every check succeeds. */
#include "ugfx.h"

static int pass, total;

static void check(int cond, const char *name) {
    total++;
    if (cond)
        pass++;
    else
        uprintf("gfxcap: FAIL %s\n", name);
}

/* Reconstruct a backbuffer pixel as 0x00RRGGBB (bytes are B, G, R). */
static unsigned px(ugfx_t *g, int x, int y) {
    unsigned char *p = g->back + (unsigned)y * g->pitch + (unsigned)x * g->bypp;
    return ((unsigned)p[2] << 16) | ((unsigned)p[1] << 8) | (unsigned)p[0];
}

void _start(void) {
    struct fbinfo fi;
    int have_fb = (sys_fbinfo(&fi) == 0);
    check(have_fb, "fbinfo present");
    if (!have_fb) {
        uprintf("gfxcap: no framebuffer\n");
        sys_exit(0);
    }
    check(fi.width > 0 && fi.height > 0, "geometry non-empty");
    check(fi.bpp == 24 || fi.bpp == 32, "supported depth");
    check(fi.pitch >= fi.width * (fi.bpp / 8), "pitch covers a row");

    ugfx_t g;
    check(ugfx_init(&g) == 0 && g.back != 0, "ugfx_init");
    if (!g.ok) {
        uprintf("gfxcap: ugfx_init failed\n");
        sys_exit(1);
    }
    int W = (int)g.width, H = (int)g.height;

    unsigned c1 = ugfx_rgb(10, 20, 30);
    unsigned c2 = ugfx_rgb(200, 100, 50);
    unsigned c3 = ugfx_rgb(60, 200, 120);
    unsigned white = ugfx_rgb(255, 255, 255);
    unsigned black = ugfx_rgb(0, 0, 0);

    /* clear fills the whole surface. */
    ugfx_clear(&g, c1);
    check(px(&g, 0, 0) == c1 && px(&g, W - 1, H - 1) == c1 &&
              px(&g, W / 2, H / 2) == c1,
          "clear fills surface");

    /* putpixel sets exactly one pixel. */
    ugfx_putpixel(&g, 5, 5, c2);
    check(px(&g, 5, 5) == c2 && px(&g, 6, 5) == c1, "putpixel one pixel");

    /* fillrect fills its region and nothing outside it. */
    ugfx_fillrect(&g, 100, 100, 50, 40, c3);
    check(px(&g, 100, 100) == c3 && px(&g, 149, 139) == c3 &&
              px(&g, 120, 120) == c3,
          "fillrect interior");
    check(px(&g, 150, 120) == c1 && px(&g, 100, 140) == c1,
          "fillrect bounded");

    /* Out-of-bounds draws are clipped no-ops (reaching here = no fault). */
    ugfx_putpixel(&g, -3, -3, white);
    ugfx_putpixel(&g, W + 9, H + 9, white);
    ugfx_fillrect(&g, W - 5, H - 5, 100, 100, white); /* spills off-screen */
    check(px(&g, 0, 0) == c1, "clipping leaves origin intact");
    check(px(&g, W - 1, H - 1) == white, "clipped fill still draws inside");

    /* Text: '-' has bitmap 0x7E on row 3 (cols 1..6); rest empty. */
    ugfx_clear(&g, black);
    ugfx_char(&g, 200, 200, '-', white);
    check(px(&g, 201, 203) == white, "glyph lit pixel");
    check(px(&g, 200, 200) == black && px(&g, 200, 203) == black,
          "glyph transparent pixels");

    /* Flush blits to the device: clear to a sentinel, then read (0,0) back. */
    unsigned sent = ugfx_rgb(123, 45, 67);
    ugfx_clear(&g, sent);
    check(ugfx_flush(&g) == 0, "flush");
    {
        int fd = sys_open("/dev/fb");
        unsigned char fp[4] = {0, 0, 0, 0};
        int r = sys_read(fd, (char *)fp, (int)g.bypp);
        sys_close(fd);
        unsigned got = ((unsigned)fp[2] << 16) | ((unsigned)fp[1] << 8) | fp[0];
        check(r == (int)g.bypp && got == sent, "flush reached the device");
    }

    /* Raw /dev/fb byte round-trip, independent of pixel meaning. */
    {
        int fd = sys_open("/dev/fb");
        unsigned char out[8], in[8];
        for (int i = 0; i < 8; i++) {
            out[i] = (unsigned char)(0x31 + i * 9);
            in[i] = 0;
        }
        int w = sys_writefd(fd, (const char *)out, 8);
        sys_close(fd);
        fd = sys_open("/dev/fb");
        int r = sys_read(fd, (char *)in, 8);
        sys_close(fd);
        int ok = (w == 8 && r == 8);
        for (int i = 0; i < 8; i++)
            if (in[i] != out[i])
                ok = 0;
        check(ok, "/dev/fb raw round-trip");
    }

    ugfx_free(&g);

    if (pass == total)
        uprintf("gfxcap: ALL PASS (%d tests)\n", total);
    else
        uprintf("gfxcap: %d/%d passed\n", pass, total);
    sys_exit(pass == total ? 0 : 1);
}
