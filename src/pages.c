#include "pages.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    AMBER = MAX_LEVEL, BRIGHT = 217, TEXT = 172, SOFT = 102,
    RULE = 54, GRID = 32, BLACK = 0
};

enum { TAB_Y = 225, TAB_H = 12, TAB_PAD = 3, TAB_NUMBER_WIDTH = 3 };

static const struct { int x; const char *name; } tabs[PAGE_COUNT] = {
    [MONITOR] = {11, "MONITOR"}, [EVENT_LOG] = {83, "EVENT LOG"}, [PANEL_TEST] = {167, "PANEL TEST"}
};

static int tab_width(Page page)
{
    return ((int)strlen(tabs[page].name) + TAB_NUMBER_WIDTH) * GLYPH_ADVANCE + 2 * TAB_PAD - 1;
}

Page page_at(float x, float y)
{
    if (y < TAB_Y - 1 || y >= TAB_Y + TAB_H + 1) return PAGE_COUNT;
    for (int i = 0; i < PAGE_COUNT; ++i) {
        int left = tabs[i].x - TAB_PAD;
        if (x >= left && x < left + tab_width((Page)i))
            return (Page)i;
    }
    return PAGE_COUNT;
}

static void page_frame(Raster r, Page page, bool paused)
{
    fill(r, 0, 0, PANEL_W, 17, 28);
    fill(r, 0, 16, PANEL_W, 1, RULE);
    label(r, 12, 5, 1, BRIGHT, "MORROW / FIELD TELEMETRY");
    label_right(r, 308, 5, 1, BRIGHT, "STATION 07");

    fill(r, 0, 220, PANEL_W, 1, RULE);
    for (int i = 0; i < PAGE_COUNT; ++i) {
        char tab[18];
        snprintf(tab, sizeof tab, "%02d %s", i + 1, tabs[i].name);
        int selected = (Page)i == page;
        if (selected)
            fill(r, tabs[i].x - TAB_PAD, TAB_Y, tab_width((Page)i), TAB_H, TEXT);
        label(r, tabs[i].x, TAB_Y + TAB_PAD, 1, selected ? BLACK : SOFT, tab);
    }
    label_right(r, 308, TAB_Y + TAB_PAD, 1, paused ? SOFT : TEXT, paused ? "HOLD" : "LIVE");
}

static void heading(Raster r, const char *title, const char *subtitle)
{
    label(r, 12, 27, 2, AMBER, title);
    label(r, 12, 47, 1, SOFT, subtitle);
    fill(r, 12, 60, 296, 1, RULE);
}

static float carrier(float x, float t)
{
    const float envelope = 0.34f + 0.66f * expf(-23.0f * (x - 0.51f) * (x - 0.51f));
    return envelope * sinf(38.0f * x - 3.7f * t)
         + 0.14f * sinf(88.0f * x + 1.2f * t)
         + 0.075f * cosf(141.0f * x - 2.0f * t);
}

static void monitor(Raster r, float t)
{
    char value[24];
    heading(r, "SIGNAL MONITOR", "BASEBAND RECEIVER   /   07-A");
    fill(r, 250, 31, 5, 5, BRIGHT);
    label(r, 260, 30, 1, BRIGHT, "LOCKED");
    label_right(r, 308, 47, 1, SOFT, "CH A / B");

    label(r, 12, 70, 1, BRIGHT, "CH A / WAVEFORM");
    label_right(r, 214, 70, 1, TEXT, "5 MS/DIV");
    box(r, 34, 83, 180, 70, GRID);
    for (int i = 1; i < 8; ++i)
        fill(r, 34 + i * 22, 84, 1, 68, GRID);
    for (int i = 1; i < 4; ++i)
        fill(r, 35, 83 + i * 17, 178, 1, i == 2 ? RULE : GRID);
    label(r, 12, 85, 1, SOFT, "+1");
    label(r, 18, 115, 1, SOFT, "0");
    label(r, 12, 146, 1, SOFT, "-1");
    int last_y = (int)lroundf(118.0f - carrier(0.0f, t) * 27.0f);
    for (int x = 1; x < 178; ++x) {
        float sample = carrier(x / 177.0f, t);
        int y = (int)lroundf(118.0f - sample * 27.0f);
        line(r, 34 + x, last_y, 35 + x, y, AMBER);
        last_y = y;
    }

    label(r, 12, 165, 1, BRIGHT, "CH B / SPECTRUM");
    label_right(r, 214, 165, 1, TEXT, "0-4 KHZ");
    fill(r, 12, 208, 202, 1, RULE);
    fill(r, 12, 193, 202, 1, GRID);
    for (int i = 0; i < 25; ++i) {
        float x = (i - 8.0f) / 4.5f;
        int h = 4 + (int)(22.0f * expf(-x * x) + 4.5f * (1.0f + sinf(i * 2.91f + 4.2f * t)));
        fill(r, 13 + i * 8, 207 - h, 6, h, i < 14 ? TEXT : SOFT);
        fill(r, 13 + i * 8, 206 - h, 6, 1, BRIGHT);
    }

    fill(r, 226, 69, 1, 140, RULE);
    label(r, 238, 71, 1, SOFT, "FREQUENCY");
    snprintf(value, sizeof value, "%04d", 1024 + (int)lroundf(sinf(t * 0.61f) * 2.0f));
    label(r, 238, 84, 2, AMBER, value);
    label(r, 288, 90, 1, TEXT, "HZ");

    fill(r, 238, 108, 70, 1, RULE);
    label(r, 238, 116, 1, SOFT, "INPUT GAIN");
    label(r, 238, 129, 2, AMBER, "+12");
    label(r, 278, 135, 1, TEXT, "DB");

    fill(r, 238, 153, 70, 1, RULE);
    label(r, 238, 161, 1, SOFT, "LEVEL");
    int level = 9 + (int)lroundf(sinf(t * 1.8f) * 1.6f);
    for (int i = 0; i < 12; ++i)
        fill(r, 238 + i * 6, 174, 4, 8, i < level ? BRIGHT : GRID);
    label(r, 238, 197, 1, TEXT, "LINK");
    label_right(r, 308, 197, 1, BRIGHT, "07-A");
}

static void event_log(Raster r, double t)
{
    static const char *const messages[] = {
        "RX FRAME CHECK PASSED", "REFERENCE CLOCK LOCKED", "BUFFER WRAP COMPLETE",
        "GAIN TABLE LOADED", "FRAME INDEX SYNCED", "REMOTE CARRIER PRESENT",
        "CHANNEL B SAMPLE READY", "LINK HEARTBEAT RECEIVED", "INPUT LEVEL NOMINAL",
    };
    size_t count = sizeof messages / sizeof messages[0];
    unsigned event = 41u + (unsigned)(t / 2.4);
    double age = fmod(t, 2.4);
    char value[24];

    heading(r, "EVENT LOG", "RECEIVER 07-A   /   LINK HISTORY");
    label_right(r, 308, 33, 1, TEXT, "CIRCULAR BUFFER");
    fill(r, 12, 68, 296, 12, TEXT);
    label(r, 16, 71, 1, BLACK, "TIME");
    label(r, 77, 71, 1, BLACK, "EVENT");
    label(r, 273, 71, 1, BLACK, "SEQ");

    for (int row = 0; row < 8; ++row) {
        unsigned id = event - 7u + (unsigned)row;
        int y = 86 + row * 15;
        int level = row == 7 ? BRIGHT : TEXT;
        if (row == 7)
            fill(r, 12, y - 3, 296, 12, 28);
        else
            fill(r, 12, y + 10, 296, 1, GRID);

        unsigned seconds = (id * 12u / 5u) % 3600u;
        snprintf(value, sizeof value, "%02u:%02u:%02u", 7u, seconds / 60u, seconds % 60u);
        label(r, 16, y, 1, row == 7 ? TEXT : SOFT, value);
        snprintf(value, sizeof value, "%04X", id & 0xffffu);
        label(r, 273, y, 1, SOFT, value);

        const char *message = messages[id % count];
        int chars = (int)strlen(message);
        if (row == 7 && age < 0.78)
            chars = (int)((age / 0.78) * chars);
        snprintf(value, sizeof value, "%.*s", chars, message);
        label(r, 77, y, 1, level, value);
        if (row == 7 && fmod(age, 0.8) < 0.4)
            fill(r, 77 + chars * 6, y, 3, 7, BRIGHT);
    }

    fill(r, 12, 211, 296, 1, RULE);
    label_right(r, 308, 47, 1, SOFT, "8 RECORDS");
}

static void panel_test(Raster r, double t)
{
    bool invert = fmod(t, 1.6) < 0.8;
    char matrix[48], drive[20];
    snprintf(matrix, sizeof matrix, "%d X %d MATRIX   /   OPTICAL CHECK", PANEL_W, PANEL_H);
    snprintf(drive, sizeof drive, "%d HZ DRIVE", PANEL_HZ);
    heading(r, "PANEL TEST", matrix);
    label_right(r, 308, 33, 1, TEXT, drive);

    label(r, 12, 69, 1, BRIGHT, "01  ADDRESS GRID");
    label(r, 166, 69, 1, BRIGHT, "02  DOT PITCH");
    box(r, 12, 81, 142, 68, RULE);
    box(r, 166, 81, 142, 68, RULE);
    for (int y = 89; y <= 141; y += 8)
        fill(r, 19, y, 128, 1, SOFT);
    for (int x = 19; x < 147; x += 8)
        fill(r, x, 89, 1, 56, SOFT);
    fill(r, 79, 87, 1, 58, BRIGHT);
    fill(r, 17, 113, 132, 1, BRIGHT);

    for (int y = 88; y < 142; ++y) {
        for (int x = 174; x < 300; ++x) {
            int pitch = x < 216 ? 1 : (x < 258 ? 2 : 4);
            if (((x / pitch + y / pitch) & 1) == invert)
                pixel(r, x, y, BRIGHT);
        }
    }
    fill(r, 216, 88, 1, 54, BLACK);
    fill(r, 258, 88, 1, 54, BLACK);

    label(r, 12, 161, 1, BRIGHT, "03  LEVEL STEPS");
    label(r, 166, 161, 1, BRIGHT, "04  LOAD REVERSAL");
    box(r, 12, 174, 142, 35, RULE);
    box(r, 166, 174, 142, 35, RULE);
    for (int i = 0; i < 8; ++i)
        fill(r, 19 + i * 16, 181, 16, 21, 12 + i * 34);
    int left = invert ? AMBER : 17;
    int right = invert ? 17 : AMBER;
    fill(r, 174, 181, 63, 21, left);
    fill(r, 237, 181, 63, 21, right);
    label(r, 195, 188, 1, right, "A");
    label(r, 269, 188, 1, left, "B");
}

float draw_page(Raster r, Page page, double time, bool paused)
{
    memset(r.pixels, 0, (size_t)r.w * r.h);
    page_frame(r, page, paused);
    switch (page) {
        case MONITOR: monitor(r, (float)time); break;
        case EVENT_LOG: event_log(r, time); break;
        case PANEL_TEST: panel_test(r, time); break;
        default: break;
    }

    unsigned load = 0;
    for (int i = 0; i < r.w * r.h; ++i)
        load += r.pixels[i];
    return (float)load / ((float)MAX_LEVEL * r.w * r.h);
}

void draw_case_labels(Raster r)
{
    const int s = INK_SCALE;
    const int small = 3;
    char drive[32];
    snprintf(drive, sizeof drive, "%d X %d  /  %d HZ", PANEL_W, PANEL_H, PANEL_HZ);
    label(r, (GLASS_X - INK_X) * s, 44 * s, 2 * s, 228, "MORROW");
    label(r, (GLASS_X + 87 - INK_X) * s, 48 * s, small, 145, "INSTRUMENTS");
    label_right(r, (GLASS_X + GLASS_W - INK_X) * s, 47 * s, small, 192,
                "EL-320  /  ELECTROLUMINESCENT DISPLAY");
    label(r, (PILOT_X + 14 - INK_X) * s, (PILOT_Y - 9) * s, small, 208, "PANEL DRIVE");
    label(r, (PILOT_X + 14 - INK_X) * s, (PILOT_Y + 7) * s, small, 117, drive);

    static const struct { const char *key, *name; } buttons[BUTTON_COUNT] = {
        [BUTTON_SCOPE] = {"1", "SCOPE"}, [BUTTON_LOG] = {"2", "LOG"}, [BUTTON_TEST] = {"3", "TEST"},
        [BUTTON_WRITE] = {"R", "WRITE"}, [BUTTON_SLOW] = {"S", "SLOW"}, [BUTTON_MODE] = {"C", "MODE"},
        [BUTTON_POWER] = {"P", "POWER"}
    };
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        int x = BUTTON_X + i * BUTTON_STEP - INK_X;
        int y = (BUTTON_Y + BUTTON_TEXT_Y) * s;
        label(r, (x + BUTTON_KEY_X) * s, y, small, AMBER, buttons[i].key);
        label(r, (x + BUTTON_LABEL_X) * s, y, small, 170, buttons[i].name);
    }
}
