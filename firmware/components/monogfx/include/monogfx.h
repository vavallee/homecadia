/* Minimal 1bpp framebuffer graphics for the e-ink display.
 * Landscape row-major buffer, bit set = black. Text via the classic 5x7
 * bitmap font (integer scaling); large digits as drawn seven-segment glyphs
 * (no big font data needed).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *buf; /* (width*height)/8 bytes, row-major, MSB = leftmost pixel */
} monogfx_t;

void monogfx_init(monogfx_t *g, uint16_t width, uint16_t height, uint8_t *buf);
void monogfx_clear(monogfx_t *g);
void monogfx_set_pixel(monogfx_t *g, int x, int y, bool black);
void monogfx_fill_rect(monogfx_t *g, int x, int y, int w, int h, bool black);

/* 5x7 font, scaled. Advance is 6*scale per character. */
void monogfx_draw_char(monogfx_t *g, int x, int y, char c, int scale);
void monogfx_draw_text(monogfx_t *g, int x, int y, const char *s, int scale);
int monogfx_text_width(const char *s, int scale);

/* Seven-segment style glyph for '0'-'9', '-' and ' '. w/h outer size,
 * thick = segment thickness. Returns the x advance (w + thick/2). */
int monogfx_draw_seg(monogfx_t *g, int x, int y, int w, int h, int thick, char c);

#ifdef __cplusplus
}
#endif
