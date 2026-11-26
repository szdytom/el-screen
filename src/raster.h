#ifndef EL_RASTER_H
#define EL_RASTER_H

#include <stdint.h>

typedef struct {
    uint8_t *pixels;
    int w, h;
} Raster;

enum { GLYPH_ADVANCE = 6 };

void pixel(Raster r, int x, int y, int level);
void fill(Raster r, int x, int y, int w, int h, int level);
void box(Raster r, int x, int y, int w, int h, int level);
void line(Raster r, int x0, int y0, int x1, int y1, int level);
void label(Raster r, int x, int y, int scale, int level, const char *text);
void label_right(Raster r, int x, int y, int scale, int level, const char *text);

#endif
