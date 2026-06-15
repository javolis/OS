/* avwall.h - the Avolis "AI network" wallpaper: a warm radial vignette with
 * a constellation of glowing orange nodes joined by faint mesh lines, echoing
 * the Avolis web hero. Deterministic (seeded) so it renders identically each
 * frame (looks static, no flicker). Drawn straight into the backbuffer. */
#pragma once
#include "ugfx.h"

#define AVW_NODES 72

static inline void avwall_render(ugfx_t *g, unsigned seed, unsigned warm) {
    int W = (int)g->width, H = (int)g->height;
    int cx = W / 2, cy = H / 2;
    int maxd2 = cx * cx + cy * cy;
    if (maxd2 < 1)
        maxd2 = 1;
    int er = 8, eg = 5, eb = 3; /* near-black warm edge */
    int wr = (int)((warm >> 16) & 0xFF), wgc = (int)((warm >> 8) & 0xFF),
        wb = (int)(warm & 0xFF);

    /* Warm radial vignette: brightest warm at the center, fading to the edge. */
    for (int y = 0; y < H; y++) {
        int ddy = y - cy;
        for (int x = 0; x < W; x++) {
            int ddx = x - cx;
            int f = 255 - (int)(255LL * (ddx * ddx + ddy * ddy) / maxd2);
            if (f < 0)
                f = 0;
            unsigned r = (unsigned)(er + (wr - er) * f / 255);
            unsigned gg = (unsigned)(eg + (wgc - eg) * f / 255);
            unsigned b = (unsigned)(eb + (wb - eb) * f / 255);
            ugfx_putpixel(g, x, y, (r << 16) | (gg << 8) | b);
        }
    }

    int nx[AVW_NODES], ny[AVW_NODES];
    unsigned s = seed ? seed : 1u;
    for (int i = 0; i < AVW_NODES; i++) {
        s = s * 1103515245u + 12345u;
        nx[i] = (int)((s >> 16) % (unsigned)W);
        s = s * 1103515245u + 12345u;
        ny[i] = (int)((s >> 16) % (unsigned)H);
    }

    unsigned linec = 0x00c8641e; /* dim orange mesh */
    unsigned nodec = 0x00ff8c2a; /* bright orange node */
    int D = (W < H ? W : H) / 5;
    if (D < 120)
        D = 120;
    int D2 = D * D;
    for (int i = 0; i < AVW_NODES; i++)
        for (int j = i + 1; j < AVW_NODES; j++) {
            int dx = nx[i] - nx[j], dy = ny[i] - ny[j];
            int d2 = dx * dx + dy * dy;
            if (d2 < D2) {
                unsigned a = (unsigned)(72 - 72 * d2 / D2);
                if (a > 6)
                    ugfx_line(g, nx[i], ny[i], nx[j], ny[j], linec, a);
            }
        }
    for (int i = 0; i < AVW_NODES; i++)
        ugfx_glow_dot(g, nx[i], ny[i], 4, nodec);
}
