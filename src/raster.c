#include "raster.h"

#include <stdlib.h>
#include <string.h>

/* Five dots per row, most significant dot first. */
static const uint8_t glyphs[96][7] = {
    ['A'-32] = {14,17,17,31,17,17,17}, ['B'-32] = {30,17,17,30,17,17,30},
    ['C'-32] = {14,17,16,16,16,17,14}, ['D'-32] = {30,17,17,17,17,17,30},
    ['E'-32] = {31,16,16,30,16,16,31}, ['F'-32] = {31,16,16,30,16,16,16},
    ['G'-32] = {14,17,16,23,17,17,15}, ['H'-32] = {17,17,17,31,17,17,17},
    ['I'-32] = {14,4,4,4,4,4,14},      ['J'-32] = {7,2,2,2,2,18,12},
    ['K'-32] = {17,18,20,24,20,18,17}, ['L'-32] = {16,16,16,16,16,16,31},
    ['M'-32] = {17,27,21,21,17,17,17}, ['N'-32] = {17,25,21,19,17,17,17},
    ['O'-32] = {14,17,17,17,17,17,14}, ['P'-32] = {30,17,17,30,16,16,16},
    ['Q'-32] = {14,17,17,17,21,18,13}, ['R'-32] = {30,17,17,30,20,18,17},
    ['S'-32] = {15,16,16,14,1,1,30},   ['T'-32] = {31,4,4,4,4,4,4},
    ['U'-32] = {17,17,17,17,17,17,14}, ['V'-32] = {17,17,17,17,17,10,4},
    ['W'-32] = {17,17,17,21,21,21,10}, ['X'-32] = {17,17,10,4,10,17,17},
    ['Y'-32] = {17,17,10,4,4,4,4},     ['Z'-32] = {31,1,2,4,8,16,31},
    ['0'-32] = {14,17,19,21,25,17,14}, ['1'-32] = {4,12,4,4,4,4,14},
    ['2'-32] = {14,17,1,2,4,8,31},     ['3'-32] = {30,1,1,14,1,1,30},
    ['4'-32] = {2,6,10,18,31,2,2},     ['5'-32] = {31,16,16,30,1,1,30},
    ['6'-32] = {14,16,16,30,17,17,14}, ['7'-32] = {31,1,2,4,8,8,8},
    ['8'-32] = {14,17,17,14,17,17,14}, ['9'-32] = {14,17,17,15,1,1,14},
    ['-'-32] = {0,0,0,31,0,0,0},
    ['/'-32] = {1,1,2,4,8,16,16},      ['.'-32] = {0,0,0,0,0,12,12},
    [':'-32] = {0,12,12,0,12,12,0},    ['+'-32] = {0,4,4,31,4,4,0},
    ['='-32] = {0,0,31,0,31,0,0},      ['>'-32] = {16,8,4,2,4,8,16},
    ['['-32] = {14,8,8,8,8,8,14},     [']'-32] = {14,2,2,2,2,2,14},
    ['%'-32] = {17,2,4,8,16,17,0},     ['_' -32] = {0,0,0,0,0,0,31},
};

void pixel(Raster r, int x, int y, int level)
{
    if ((unsigned)x < (unsigned)r.w && (unsigned)y < (unsigned)r.h)
        r.pixels[y * r.w + x] = (uint8_t)level;
}

void fill(Raster r, int x, int y, int w, int h, int level)
{
    int right = x + w < r.w ? x + w : r.w;
    int bottom = y + h < r.h ? y + h : r.h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right <= x || bottom <= y) return;
    for (int row = y; row < bottom; ++row)
        memset(r.pixels + row * r.w + x, level, (size_t)(right - x));
}

void box(Raster r, int x, int y, int w, int h, int level)
{
    fill(r, x, y, w, 1, level);
    fill(r, x, y + h - 1, w, 1, level);
    fill(r, x, y, 1, h, level);
    fill(r, x + w - 1, y, 1, h, level);
}

void line(Raster r, int x0, int y0, int x1, int y1, int level)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        pixel(r, x0, y0, level);
        if (x0 == x1 && y0 == y1) break;
        int step = 2 * error;
        if (step >= dy) { error += dy; x0 += sx; }
        if (step <= dx) { error += dx; y0 += sy; }
    }
}

void label(Raster r, int x, int y, int scale, int level, const char *text)
{
    for (const unsigned char *c = (const unsigned char *)text; *c; ++c, x += GLYPH_ADVANCE * scale) {
        unsigned index = (unsigned)*c - 32u;
        if (index >= sizeof glyphs / sizeof glyphs[0]) continue;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyphs[index][row] & (16 >> col))
                    fill(r, x + col * scale, y + row * scale, scale, scale, level);
            }
        }
    }
}

void label_right(Raster r, int x, int y, int scale, int level, const char *text)
{
    label(r, x - ((int)strlen(text) * GLYPH_ADVANCE - 1) * scale, y, scale, level, text);
}
