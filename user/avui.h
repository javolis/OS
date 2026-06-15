/* avui.h - the Avolis look: palette and a few widget helpers on top of ugfx
 * and the anti-aliased font. Black/orange/white, dark, spacious. Every
 * Avolis screen draws with these. */
#pragma once
#include "uafont.h"
#include "ugfx.h"

/* Palette (functions, so evaluated at use; not compile-time constants). */
#define AV_BG ugfx_rgb(13, 13, 13)       /* desktop / deepest background */
#define AV_PANEL ugfx_rgb(22, 22, 22)    /* surfaces, taskbar, tiles */
#define AV_PANEL2 ugfx_rgb(32, 32, 32)   /* raised / hovered surface */
#define AV_ORANGE ugfx_rgb(255, 122, 26) /* primary accent */
#define AV_ORANGE_DIM ugfx_rgb(180, 86, 18)
#define AV_WHITE ugfx_rgb(242, 242, 242) /* primary text */
#define AV_GRAY ugfx_rgb(150, 150, 150)  /* secondary text */
#define AV_DIM ugfx_rgb(92, 92, 92)      /* hints */
#define AV_BORDER ugfx_rgb(44, 44, 44)   /* subtle borders */

/* A surface panel with rounded corners; when focused, an orange ring. */
static inline void av_panel(ugfx_t *g, int x, int y, int w, int h,
                            int focused) {
    if (focused)
        ugfx_round_rect(g, x - 2, y - 2, w + 4, h + 4, 10, AV_ORANGE);
    ugfx_round_rect(g, x, y, w, h, 8, AV_PANEL);
}

/* Body / heading text in a chosen color, baseline at y. */
static inline void av_text(ugfx_t *g, int x, int y, const char *s,
                           unsigned rgb) {
    ua_text(g, UAFONT_BODY, x, y, s, rgb);
}
static inline void av_head(ugfx_t *g, int x, int y, const char *s,
                           unsigned rgb) {
    ua_text(g, UAFONT_HEAD, x, y, s, rgb);
}

/* Text with a soft glow halo (the Avolis hero look): the string is drawn in a
 * dimmed color at 8 small offsets to build a halo, then crisp on top. */
static inline void av_text_glow(ugfx_t *g, int fi, int x, int baseline,
                                const char *s, unsigned rgb) {
    unsigned dim = ((((rgb >> 16) & 0xFF) * 38 / 100) << 16) |
                   ((((rgb >> 8) & 0xFF) * 38 / 100) << 8) |
                   (((rgb & 0xFF) * 38 / 100));
    static const int ox[8] = {-2, 2, 0, 0, -2, 2, -2, 2};
    static const int oy[8] = {0, 0, -2, 2, -2, -2, 2, 2};
    for (int i = 0; i < 8; i++)
        ua_text(g, fi, x + ox[i], baseline + oy[i], s, dim);
    ua_text(g, fi, x, baseline, s, rgb);
}

/* A filled pill the focus uses for selected rows/buttons. */
static inline void av_button(ugfx_t *g, int x, int y, int w, int h,
                             const char *label, int focused) {
    ugfx_round_rect(g, x, y, w, h, h / 2, focused ? AV_ORANGE : AV_PANEL2);
    int baseline = y + h / 2 + ua_height(UAFONT_BODY) / 2 - 4;
    unsigned fg = focused ? ugfx_rgb(26, 16, 3) : AV_WHITE; /* dark on orange */
    ua_text_center(g, UAFONT_BODY, x, w, baseline, label, fg);
}
