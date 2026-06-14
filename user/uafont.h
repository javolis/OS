/* uafont.h - anti-aliased text for ugfx, using the generated coverage font
 * (include/font_aa.h). Glyphs are blended onto the backbuffer with
 * proportional advance, so text looks smooth rather than blocky. Include
 * this (instead of just ugfx.h) in programs that want quality text; it
 * pulls in the large font data, so only the desktop shell and friends pay
 * for it. */
#pragma once
#include "font_aa.h"
#include "ugfx.h"

#define UAFONT_BODY 0
#define UAFONT_HEAD 1

/* Width in pixels that ua_text would advance for the string at font size fi. */
static inline int ua_text_width(int fi, const char *s) {
    if (fi < 0 || fi >= AAFONT_NSIZES)
        fi = 0;
    const struct aafont *F = &aafonts[fi];
    int w = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < AAFONT_FIRST || c > AAFONT_LAST)
            c = ' ';
        w += F->glyphs[c - AAFONT_FIRST].advance;
    }
    return w;
}

/* The line height (cell height) of font size fi, for vertical layout. */
static inline int ua_height(int fi) {
    if (fi < 0 || fi >= AAFONT_NSIZES)
        fi = 0;
    return aafonts[fi].height;
}

/* Draw s in color rgb with x as the left edge and `baseline` as the text
 * baseline. Returns the advance width drawn. */
static inline int ua_text(ugfx_t *g, int fi, int x, int baseline,
                          const char *s, unsigned rgb) {
    if (fi < 0 || fi >= AAFONT_NSIZES)
        fi = 0;
    const struct aafont *F = &aafonts[fi];
    int pen = x;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < AAFONT_FIRST || c > AAFONT_LAST)
            c = ' ';
        const struct aaglyph *gl = &F->glyphs[c - AAFONT_FIRST];
        const uint8_t *cov = F->cov + gl->off;
        for (int yy = 0; yy < gl->h; yy++)
            for (int xx = 0; xx < gl->w; xx++) {
                uint8_t a = cov[yy * gl->w + xx];
                if (a)
                    ugfx_blend_pixel(g, pen + gl->xoff + xx,
                                     baseline + gl->yoff + yy, rgb, a);
            }
        pen += gl->advance;
    }
    return pen - x;
}

/* Convenience: draw centered horizontally within [x, x+w). */
static inline void ua_text_center(ugfx_t *g, int fi, int x, int w,
                                  int baseline, const char *s, unsigned rgb) {
    int tw = ua_text_width(fi, s);
    ua_text(g, fi, x + (w - tw) / 2, baseline, s, rgb);
}
