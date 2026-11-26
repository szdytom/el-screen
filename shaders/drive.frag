#version 300 es
precision highp float;
precision highp int;

uniform sampler2D uVideo;
uniform sampler2D uPrevious;
uniform float uStep;
uniform float uPhase;
uniform float uPeriod;
uniform float uTransitionAge;
uniform int uTransition;
uniform bool uPower;
uniform vec2 uMemoryStep;
out vec4 state;

void main()
{
    ivec2 cell = ivec2(gl_FragCoord.xy);
    float row = gl_FragCoord.y / float(textureSize(uVideo, 0).y);
    vec4 old = texelFetch(uPrevious, cell, 0);
    float held = old.r;

    // Recover the last address pulse for this row, even when a render frame
    // spans multiple panel refreshes. Texture row zero is the top of the glass.
    float sincePulse = fract(uPhase - row) * uPeriod;
    bool addressed = sincePulse < uStep;
    if (addressed) {
        float video = texelFetch(uVideo, cell, 0).r;
        float atPulse = uTransitionAge - sincePulse;
        if (!uPower) {
            held = 0.0;
        } else if (uTransition == WAKE_MODE) {
            held = atPulse >= WAKE_DELAY + row * WAKE_SWEEP ? video : 0.0;
        } else if (uTransition == REWRITE_MODE) {
            if (atPulse >= WRITE_DELAY + row * WRITE_SWEEP)
                held = video;
            else if (atPulse >= row * CLEAR_SWEEP)
                held = 0.0;
        } else {
            held = video;
        }
    }

    // RGB encode held drive, weak fall tail, and the remaining rise deficit.
    // Bias the decays toward zero so RGBA8 rounding cannot leave a stuck tail.
    const float decayBias = HISTORY_DECAY_BIAS_CODES / float(MAX_LEVEL);
    float tail = max(old.g * exp(-uStep / PIXEL_FALL) - decayBias, 0.0);
    float attack = max(old.b * exp(-uStep / PIXEL_RISE) - decayBias, 0.0);
    if (held < old.r)
        tail = max(tail, (old.r - held) * PIXEL_TAIL_GAIN * exp(-sincePulse / PIXEL_FALL));
    if (held > old.r) {
        tail = 0.0;
        attack = (held - old.r) * exp(-sincePulse / PIXEL_RISE);
    }

    // Alpha accumulates exposure and releases it more slowly than the rise.
    // The capture profile uses this channel for the retained image.
    float memory = uPower ? clamp(held, old.a - uMemoryStep.y, old.a + uMemoryStep.x) : 0.0;
    state = vec4(held, tail, attack, memory);
}
