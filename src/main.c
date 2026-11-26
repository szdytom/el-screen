#include "display.h"

#include <SDL3/SDL.h>
#include <math.h>

typedef struct {
    DisplayFrame frame;
    uint8_t video[PANEL_W * PANEL_H];
    double time, video_time, drawn_at;
    float requested_load;
    bool pause, dirty;
} Demo;

static void set_page(Demo *demo, Page page)
{
    if (page == demo->frame.page) return;
    demo->frame.page = page;
    demo->dirty = true;
    if (demo->frame.power && demo->frame.transition != WAKE) {
        demo->frame.transition = REWRITE;
        demo->frame.transition_age = 0.0f;
    }
}

static void operate(Demo *demo, CaseButton button)
{
    DisplayFrame *frame = &demo->frame;
    switch (button) {
        case BUTTON_SCOPE: set_page(demo, MONITOR); break;
        case BUTTON_LOG: set_page(demo, EVENT_LOG); break;
        case BUTTON_TEST: set_page(demo, PANEL_TEST); break;
        case BUTTON_WRITE:
            if (frame->power) {
                frame->transition = REWRITE;
                frame->transition_age = 0.0f;
            }
            break;
        case BUTTON_SLOW:
            frame->slow = !frame->slow;
            break;
        case BUTTON_MODE:
            frame->rolling = !frame->rolling;
            break;
        case BUTTON_POWER:
            frame->power = !frame->power;
            frame->transition = frame->power ? WAKE : STEADY;
            frame->transition_age = 0.0f;
            break;
        case NO_BUTTON:
            break;
    }
}

static void keys(SDL_Window *window, Demo *demo, SDL_Keycode key, SDL_Keymod mod, bool *running)
{
    DisplayFrame *frame = &demo->frame;
    switch (key) {
        case SDLK_ESCAPE: *running = false; break;
        case SDLK_1: operate(demo, BUTTON_SCOPE); break;
        case SDLK_2: operate(demo, BUTTON_LOG); break;
        case SDLK_3: operate(demo, BUTTON_TEST); break;
        case SDLK_R: operate(demo, BUTTON_WRITE); break;
        case SDLK_S: operate(demo, BUTTON_SLOW); break;
        case SDLK_C: operate(demo, BUTTON_MODE); break;
        case SDLK_P: operate(demo, BUTTON_POWER); break;
        case SDLK_B: frame->bloom = !frame->bloom; break;
        case SDLK_TAB:
            set_page(demo, (frame->page + ((mod & SDL_KMOD_SHIFT) ? PAGE_COUNT - 1 : 1)) % PAGE_COUNT);
            break;
        case SDLK_SPACE:
            demo->pause = !demo->pause;
            demo->dirty = true;
            break;
        case SDLK_F:
            if (!SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)))
                SDL_Log("Could not change fullscreen mode: %s", SDL_GetError());
            break;
        case SDLK_V:
            frame->glass_only = !frame->glass_only;
            frame->hover = NO_BUTTON;
            break;
        case SDLK_EQUALS: case SDLK_KP_PLUS:
            frame->brightness = fminf(frame->brightness + BRIGHTNESS_STEP, MAX_BRIGHTNESS);
            break;
        case SDLK_MINUS: case SDLK_KP_MINUS:
            frame->brightness = fmaxf(frame->brightness - BRIGHTNESS_STEP, MIN_BRIGHTNESS);
            break;
    }
}

static CaseButton case_button(float x, float y)
{
    if (x < BUTTON_X || x >= BUTTON_X + BUTTON_COUNT * BUTTON_STEP ||
        y < BUTTON_Y || y >= BUTTON_Y + BUTTON_H)
        return NO_BUTTON;
    int dx = (int)(x - BUTTON_X);
    return dx % BUTTON_STEP < BUTTON_W ? (CaseButton)(dx / BUTTON_STEP) : NO_BUTTON;
}

static void mouse(SDL_Window *window, Demo *demo, float x, float y, bool pressed)
{
    int w, h;
    if (!SDL_GetWindowSize(window, &w, &h)) return;
    const float content_w = demo->frame.glass_only ? GLASS_W : CASE_W;
    const float content_h = demo->frame.glass_only ? GLASS_H : CASE_H;
    float scale = fminf(w / content_w, h / content_h);
    if (scale <= 0.0f) return;
    x = (x - (w - content_w * scale) * 0.5f) / scale;
    y = (y - (h - content_h * scale) * 0.5f) / scale;
    if (demo->frame.glass_only) {
        x += GLASS_X;
        y += GLASS_Y;
    }
    CaseButton button = demo->frame.glass_only ? NO_BUTTON : case_button(x, y);
    demo->frame.hover = button;
    if (!pressed) return;
    if (button != NO_BUTTON) {
        operate(demo, button);
        return;
    }
    if (!demo->frame.power) return;

    Page page = page_at((x - GLASS_X) * PANEL_W / GLASS_W,
                        (y - GLASS_Y) * PANEL_H / GLASS_H);
    if (page != PAGE_COUNT)
        set_page(demo, page);
}

static float memory_step(double from, double to, double full_scale_seconds)
{
    // Schedule whole RGBA8 code steps using absolute time so low frame
    // deltas and slow motion cannot freeze the stored exposure.
    double rate = MAX_LEVEL / full_scale_seconds;
    return (float)((floor(to * rate) - floor(from * rate)) / MAX_LEVEL);
}

static void advance(Demo *demo, Display *display, double elapsed)
{
    DisplayFrame *f = &demo->frame;
    f->step = (float)(elapsed * (f->slow ? SLOW_RATE : 1.0));
    double previous = demo->time;
    demo->time += f->step;
    f->memory_rise = memory_step(previous, demo->time, MEMORY_CHARGE);
    f->memory_fall = memory_step(previous, demo->time, MEMORY_RELEASE);
    // A slow beat moves the band upward. Both wraps happen beyond the glass.
    f->band_center = (float)(BAND_START - BAND_TRAVEL * fmod(demo->time / BAND_PERIOD + BAND_PHASE, 1.0));
    f->band_half_width = (float)(BAND_WIDTH + BAND_WIDTH_SWING * sin(demo->time * BAND_WIDTH_RATE));
    if (!demo->pause && f->power) demo->video_time += f->step;

    if (demo->dirty || demo->video_time - demo->drawn_at >= 1.0 / VIDEO_HZ) {
        demo->requested_load = draw_page((Raster){demo->video, PANEL_W, PANEL_H},
                                         f->page, demo->video_time, demo->pause);
        display_upload(display, demo->video);
        demo->drawn_at = demo->video_time;
        demo->dirty = false;
    }

    double cycles = demo->time / f->period;
    f->phase = (float)(cycles - floor(cycles));
    f->polarity = fmod(cycles, 2.0) < 1.0 ? 1.0f : -1.0f;
    float target = f->power ? 1.0f : 0.0f;
    float tau = f->power ? POWER_RISE : POWER_FALL;
    f->voltage += (target - f->voltage) * (1.0f - expf(-f->step / tau));
    f->load += (demo->requested_load * target - f->load) * (1.0f - expf(-f->step / (float)LOAD_RESPONSE));
    if (f->transition != STEADY) {
        f->transition_age += f->step;
        float write_end = f->transition == WAKE ? WAKE_DELAY + WAKE_SWEEP
                                                : fmaxf(CLEAR_SWEEP, WRITE_DELAY + WRITE_SWEEP);
        if (f->transition_age >= fmaxf(write_end, TRANSITION_SETTLE) + f->period)
            f->transition = STEADY;
    }
}

static SDL_Window *open_window(void)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    int w = CASE_W, h = CASE_H;
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)) {
        float fit = fminf((usable.w - 32.0f) / CASE_W, (usable.h - 58.0f) / CASE_H);
        if (fit > 0.0f && fit < 1.0f) {
            w = (int)(w * fit);
            h = (int)(h * fit);
        }
    }
    return SDL_CreateWindow("Morrow EL-320", w, h,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "SDL initialization: %s", SDL_GetError());
        return 1;
    }
    int status = 1;
    SDL_Window *window = open_window();
    SDL_GLContext context = window ? SDL_GL_CreateContext(window) : NULL;
    if (!context) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "OpenGL ES 3.0 window: %s", SDL_GetError());
        goto cleanup;
    }
    SDL_SetWindowMinimumSize(window, 400, 312);
    SDL_GL_SetSwapInterval(1);

    Display display = {0};
    if (display_init(&display)) {
        Demo demo = {
            .frame = {
                .period = 1.0f / PANEL_HZ, .brightness = DEFAULT_BRIGHTNESS, .transition = WAKE,
                .power = true, .bloom = true, .hover = NO_BUTTON
            },
            .dirty = true
        };
        bool running = true;
        Uint64 last = SDL_GetTicksNS();
        const Uint64 frame_budget = SDL_NS_PER_SECOND / MAX_RENDER_HZ;
        while (running) {
            Uint64 start = SDL_GetTicksNS();
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_EVENT_QUIT:
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        running = false;
                        break;
                    case SDL_EVENT_KEY_DOWN:
                        if (!event.key.repeat)
                            keys(window, &demo, event.key.key, event.key.mod, &running);
                        break;
                    case SDL_EVENT_MOUSE_MOTION:
                        mouse(window, &demo, event.motion.x, event.motion.y, false);
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                        if (event.button.button == SDL_BUTTON_LEFT)
                            mouse(window, &demo, event.button.x, event.button.y, true);
                        break;
                    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                        demo.frame.hover = NO_BUTTON;
                        break;
                }
            }
            if (!running) break;

            int w, h;
            if (!SDL_GetWindowSizeInPixels(window, &w, &h) || w <= 0 || h <= 0 ||
                (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)) {
                SDL_Delay(30);
                continue;
            }

            advance(&demo, &display, fmin((double)(start - last) / SDL_NS_PER_SECOND, MAX_FRAME_STEP));
            last = start;
            display_render(&display, &demo.frame, w, h);
            SDL_GL_SwapWindow(window);
            Uint64 spent = SDL_GetTicksNS() - start;
            if (spent < frame_budget)
                SDL_DelayPrecise(frame_budget - spent);
        }
        status = 0;
    }
    display_destroy(&display);

cleanup:
    if (context) SDL_GL_DestroyContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
