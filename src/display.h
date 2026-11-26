#ifndef EL_DISPLAY_H
#define EL_DISPLAY_H

#include "pages.h"

#include <GLES3/gl3.h>

#define DRIVE_UNIFORMS(X) \
    X(uStep) X(uPhase) X(uPeriod) X(uTransitionAge) X(uTransition) X(uPower) X(uMemoryStep)
#define PANEL_UNIFORMS(X) \
    X(uDrawable) X(uVoltage) X(uBrightness) X(uLoad) X(uPhase) X(uPolarity) \
    X(uEngaged) X(uHover) X(uGlassOnly) X(uBloom) \
    X(uRolling) X(uBand)
#define UNIFORM_FIELD(name) GLint name;

typedef struct {
    GLuint drive_program, bloom_program, panel_program, vao;
    GLuint video, silkscreen, light[2], framebuffer[2];
    GLuint glow[2], glow_framebuffer[2];
    GLint bloom_horizontal;
    int front;
    struct { DRIVE_UNIFORMS(UNIFORM_FIELD) } drive;
    struct { PANEL_UNIFORMS(UNIFORM_FIELD) } panel;
} Display;

#undef UNIFORM_FIELD

typedef enum { STEADY, REWRITE = REWRITE_MODE, WAKE = WAKE_MODE } Transition;

typedef struct {
    float step, phase, period, polarity, transition_age;
    float voltage, brightness, load;
    float memory_rise, memory_fall;
    float band_center, band_half_width;
    Transition transition;
    Page page;
    CaseButton hover;
    bool power, slow, glass_only, bloom, rolling;
} DisplayFrame;

bool display_init(Display *d);
void display_destroy(Display *d);
void display_upload(Display *d, const uint8_t *video);
void display_render(Display *d, const DisplayFrame *frame, int width, int height);

#endif
