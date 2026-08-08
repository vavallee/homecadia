#include "monogfx.h"

#include <string.h>

extern const uint8_t monogfx_font5x7[];

void monogfx_init(monogfx_t *g, uint16_t width, uint16_t height, uint8_t *buf)
{
    g->width = width;
    g->height = height;
    g->buf = buf;
    monogfx_clear(g);
}

void monogfx_clear(monogfx_t *g)
{
    memset(g->buf, 0, (size_t)g->width * g->height / 8);
}

void monogfx_set_pixel(monogfx_t *g, int x, int y, bool black)
{
    if (x < 0 || y < 0 || x >= g->width || y >= g->height) {
        return;
    }
    uint8_t *p = &g->buf[(y * g->width + x) / 8];
    uint8_t mask = 0x80 >> (x & 7);
    if (black) {
        *p |= mask;
    } else {
        *p &= ~mask;
    }
}

void monogfx_fill_rect(monogfx_t *g, int x, int y, int w, int h, bool black)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            monogfx_set_pixel(g, i, j, black);
        }
    }
}

void monogfx_draw_char(monogfx_t *g, int x, int y, char c, int scale)
{
    const uint8_t *glyph = &monogfx_font5x7[(uint8_t)c * 5];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if (glyph[col] & (1 << row)) {
                monogfx_fill_rect(g, x + col * scale, y + row * scale, scale, scale, true);
            }
        }
    }
}

void monogfx_draw_text(monogfx_t *g, int x, int y, const char *s, int scale)
{
    for (; *s; s++) {
        monogfx_draw_char(g, x, y, *s, scale);
        x += 6 * scale;
    }
}

int monogfx_text_width(const char *s, int scale)
{
    return (int)strlen(s) * 6 * scale;
}

/* Segment bits: 0=top 1=top-right 2=bottom-right 3=bottom 4=bottom-left
 * 5=top-left 6=middle */
static const uint8_t k_seg_map[10] = {
    0x3F, /* 0 */ 0x06, /* 1 */ 0x5B, /* 2 */ 0x4F, /* 3 */ 0x66, /* 4 */
    0x6D, /* 5 */ 0x7D, /* 6 */ 0x07, /* 7 */ 0x7F, /* 8 */ 0x6F, /* 9 */
};

int monogfx_draw_seg(monogfx_t *g, int x, int y, int w, int h, int thick, char c)
{
    uint8_t segs;
    if (c >= '0' && c <= '9') {
        segs = k_seg_map[c - '0'];
    } else if (c == '-') {
        segs = 0x40;
    } else { /* space or unknown: advance only */
        return w + thick / 2;
    }

    int half = h / 2;
    if (segs & 0x01) monogfx_fill_rect(g, x, y, w, thick, true);                            /* top */
    if (segs & 0x02) monogfx_fill_rect(g, x + w - thick, y, thick, half, true);             /* top-right */
    if (segs & 0x04) monogfx_fill_rect(g, x + w - thick, y + half, thick, h - half, true);  /* bottom-right */
    if (segs & 0x08) monogfx_fill_rect(g, x, y + h - thick, w, thick, true);                /* bottom */
    if (segs & 0x10) monogfx_fill_rect(g, x, y + half, thick, h - half, true);              /* bottom-left */
    if (segs & 0x20) monogfx_fill_rect(g, x, y, thick, half, true);                         /* top-left */
    if (segs & 0x40) monogfx_fill_rect(g, x, y + half - thick / 2, w, thick, true);         /* middle */
    return w + thick / 2;
}
