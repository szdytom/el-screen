#ifndef EL_PAGES_H
#define EL_PAGES_H

#include "raster.h"
#include "tuning.h"
#include <stdbool.h>

typedef enum { MONITOR, EVENT_LOG, PANEL_TEST, PAGE_COUNT } Page;

typedef enum {
    NO_BUTTON = -1,
    BUTTON_SCOPE, BUTTON_LOG, BUTTON_TEST, BUTTON_WRITE,
    BUTTON_SLOW, BUTTON_MODE, BUTTON_POWER
} CaseButton;

_Static_assert(BUTTON_POWER + 1 == BUTTON_COUNT, "Update the faceplate button count");

Page page_at(float x, float y);
float draw_page(Raster r, Page page, double time, bool paused);
void draw_case_labels(Raster r);

#endif
