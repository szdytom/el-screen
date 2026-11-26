#include "display.h"
#include "shader_sources.h"

#include <SDL3/SDL.h>
#include <math.h>

enum { BLOOM_W = PANEL_W / BLOOM_DOWNSAMPLE, BLOOM_H = PANEL_H / BLOOM_DOWNSAMPLE };

static GLuint compile(GLenum type, const char *source, const char *name)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &source, NULL);
    glCompileShader(id);
    GLint status;
    glGetShaderiv(id, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) return id;

    char log[4096] = {0};
    glGetShaderInfoLog(id, sizeof log, NULL, log);
    SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s: %s", name, log);
    glDeleteShader(id);
    return 0;
}

static GLuint link(GLuint vertex, const char *fragment, const char *name)
{
    GLuint shader = compile(GL_FRAGMENT_SHADER, fragment, name);
    if (!shader) return 0;

    GLuint id = glCreateProgram();
    glAttachShader(id, vertex);
    glAttachShader(id, shader);
    glLinkProgram(id);
    glDeleteShader(shader);
    GLint status;
    glGetProgramiv(id, GL_LINK_STATUS, &status);
    if (status == GL_TRUE) return id;

    char log[4096] = {0};
    glGetProgramInfoLog(id, sizeof log, NULL, log);
    SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s: %s", name, log);
    glDeleteProgram(id);
    return 0;
}

static GLuint texture(GLenum format, GLint filter, int w, int h)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return id;
}

static bool attach_target(GLuint framebuffer, GLuint image)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

static void set_bloom_weights(GLuint program)
{
    const float sigma[2] = {BLOOM_NEAR_SIGMA, BLOOM_WIDE_SIGMA};
    float weight[BLOOM_RADIUS + 1][2];
    for (int band = 0; band < 2; ++band) {
        float sum = 1.0f;
        weight[0][band] = 1.0f;
        for (int i = 1; i <= BLOOM_RADIUS; ++i) {
            float distance = i / sigma[band];
            weight[i][band] = expf(-0.5f * distance * distance);
            sum += 2.0f * weight[i][band];
        }
        for (int i = 0; i <= BLOOM_RADIUS; ++i)
            weight[i][band] /= sum;
    }
    glUniform2fv(glGetUniformLocation(program, "uWeight[0]"), BLOOM_RADIUS + 1, weight[0]);
}

bool display_init(Display *d)
{
    GLuint vertex = compile(GL_VERTEX_SHADER, vertex_source, "fullscreen.vert");
    if (!vertex) return false;
    d->drive_program = link(vertex, drive_source, "drive.frag");
    d->bloom_program = link(vertex, bloom_source, "bloom.frag");
    d->panel_program = link(vertex, panel_source, "panel.frag");
    glDeleteShader(vertex);
    if (!d->drive_program || !d->bloom_program || !d->panel_program) return false;

    glGenVertexArrays(1, &d->vao);
    glBindVertexArray(d->vao);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    d->video = texture(GL_R8, GL_NEAREST, PANEL_W, PANEL_H);
    const int ink_w = INK_W * INK_SCALE;
    const int ink_h = CASE_H * INK_SCALE;
    d->silkscreen = texture(GL_R8, GL_LINEAR, ink_w, ink_h);
    uint8_t *art = SDL_calloc((size_t)ink_w * ink_h, 1);
    if (!art) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Could not allocate front panel artwork");
        return false;
    }
    draw_case_labels((Raster){art, ink_w, ink_h});
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ink_w, ink_h, GL_RED, GL_UNSIGNED_BYTE, art);
    SDL_free(art);

    glGenFramebuffers(2, d->framebuffer);
    for (int i = 0; i < 2; ++i) {
        d->light[i] = texture(GL_RGBA8, GL_LINEAR, PANEL_W, PANEL_H);
        if (!attach_target(d->framebuffer[i], d->light[i])) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Could not allocate panel history framebuffer");
            return false;
        }
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glGenFramebuffers(2, d->glow_framebuffer);
    for (int i = 0; i < 2; ++i) {
        d->glow[i] = texture(GL_RG8, GL_LINEAR, BLOOM_W, BLOOM_H);
        if (!attach_target(d->glow_framebuffer[i], d->glow[i])) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Could not allocate bloom framebuffer");
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#define DRIVE_LOCATION(name) d->drive.name = glGetUniformLocation(d->drive_program, #name);
#define PANEL_LOCATION(name) d->panel.name = glGetUniformLocation(d->panel_program, #name);
    DRIVE_UNIFORMS(DRIVE_LOCATION)
    PANEL_UNIFORMS(PANEL_LOCATION)
#undef DRIVE_LOCATION
#undef PANEL_LOCATION

    glUseProgram(d->drive_program);
    glUniform1i(glGetUniformLocation(d->drive_program, "uVideo"), 0);
    glUniform1i(glGetUniformLocation(d->drive_program, "uPrevious"), 1);
    glUseProgram(d->bloom_program);
    glUniform1i(glGetUniformLocation(d->bloom_program, "uImage"), 0);
    glUniform2f(glGetUniformLocation(d->bloom_program, "uSize"), BLOOM_W, BLOOM_H);
    set_bloom_weights(d->bloom_program);
    d->bloom_horizontal = glGetUniformLocation(d->bloom_program, "uHorizontal");
    glUseProgram(d->panel_program);
    glUniform1i(glGetUniformLocation(d->panel_program, "uLight"), 0);
    glUniform1i(glGetUniformLocation(d->panel_program, "uSilkscreen"), 1);
    glUniform1i(glGetUniformLocation(d->panel_program, "uBloomMap"), 2);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Could not initialize OpenGL ES resources (0x%x)", error);
        return false;
    }
    return true;
}

void display_destroy(Display *d)
{
    glDeleteFramebuffers(2, d->framebuffer);
    glDeleteFramebuffers(2, d->glow_framebuffer);
    glDeleteTextures(2, d->light);
    glDeleteTextures(2, d->glow);
    glDeleteTextures(1, &d->video);
    glDeleteTextures(1, &d->silkscreen);
    glDeleteVertexArrays(1, &d->vao);
    glDeleteProgram(d->drive_program);
    glDeleteProgram(d->bloom_program);
    glDeleteProgram(d->panel_program);
}

void display_upload(Display *d, const uint8_t *video)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, d->video);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PANEL_W, PANEL_H, GL_RED, GL_UNSIGNED_BYTE, video);
}

static void bind_inputs(GLuint source, GLuint auxiliary)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, auxiliary);
}

static void render_bloom(const Display *d)
{
    glViewport(0, 0, BLOOM_W, BLOOM_H);
    glUseProgram(d->bloom_program);
    glActiveTexture(GL_TEXTURE0);

    glBindFramebuffer(GL_FRAMEBUFFER, d->glow_framebuffer[0]);
    glBindTexture(GL_TEXTURE_2D, d->light[d->front]);
    glUniform1i(d->bloom_horizontal, true);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, d->glow_framebuffer[1]);
    glBindTexture(GL_TEXTURE_2D, d->glow[0]);
    glUniform1i(d->bloom_horizontal, false);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

static int engaged_buttons(const DisplayFrame *frame)
{
    static const CaseButton page_button[PAGE_COUNT] = {
        [MONITOR] = BUTTON_SCOPE, [EVENT_LOG] = BUTTON_LOG, [PANEL_TEST] = BUTTON_TEST
    };
    int engaged = 0;
    if (frame->power)
        engaged |= (1 << page_button[frame->page]) | (1 << BUTTON_POWER);
    if (frame->transition == REWRITE) engaged |= 1 << BUTTON_WRITE;
    if (frame->slow) engaged |= 1 << BUTTON_SLOW;
    if (frame->rolling) engaged |= 1 << BUTTON_MODE;
    return engaged;
}

void display_render(Display *d, const DisplayFrame *frame, int width, int height)
{
    int back = 1 - d->front;
    glBindFramebuffer(GL_FRAMEBUFFER, d->framebuffer[back]);
    glViewport(0, 0, PANEL_W, PANEL_H);
    glUseProgram(d->drive_program);
    bind_inputs(d->video, d->light[d->front]);
    glUniform1f(d->drive.uStep, frame->step);
    glUniform1f(d->drive.uPhase, frame->phase);
    glUniform1f(d->drive.uPeriod, frame->period);
    glUniform1f(d->drive.uTransitionAge, frame->transition_age);
    glUniform1i(d->drive.uTransition, frame->transition);
    glUniform1i(d->drive.uPower, frame->power);
    glUniform2f(d->drive.uMemoryStep, frame->memory_rise, frame->memory_fall);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    d->front = back;

    if (frame->bloom)
        render_bloom(d);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glUseProgram(d->panel_program);
    bind_inputs(d->light[d->front], d->silkscreen);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, d->glow[1]);
    glUniform2f(d->panel.uDrawable, (float)width, (float)height);
    glUniform1f(d->panel.uVoltage, frame->voltage);
    glUniform1f(d->panel.uBrightness, frame->brightness);
    glUniform1f(d->panel.uLoad, frame->load);
    glUniform1f(d->panel.uPhase, frame->phase);
    glUniform1f(d->panel.uPolarity, frame->polarity);
    glUniform1i(d->panel.uEngaged, engaged_buttons(frame));
    glUniform1i(d->panel.uHover, frame->hover);
    glUniform1i(d->panel.uGlassOnly, frame->glass_only);
    glUniform1i(d->panel.uBloom, frame->bloom);
    glUniform1i(d->panel.uRolling, frame->rolling);
    glUniform2f(d->panel.uBand, frame->band_center, frame->band_half_width);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
