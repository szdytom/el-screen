#version 300 es
precision highp float;
precision highp int;

uniform sampler2D uLight;
uniform sampler2D uSilkscreen;
uniform sampler2D uBloomMap;
uniform vec2 uDrawable;
uniform float uVoltage;
uniform float uBrightness;
uniform float uLoad;
uniform float uPhase;
uniform float uPolarity;
uniform int uEngaged;
uniform int uHover;
uniform bool uGlassOnly;
uniform bool uBloom;
uniform bool uRolling;
uniform vec2 uBand;

out vec4 fragColor;

const vec2 caseSize = vec2(CASE_W, CASE_H);
const vec2 glassOrigin = vec2(GLASS_X, GLASS_Y);
const vec2 glassSize = vec2(GLASS_W, GLASS_H);

float grain(vec2 p)
{
    vec3 n = fract(vec3(p.xyx) * 0.1031);
    n += dot(n, n.yzx + 33.33);
    return fract((n.x + n.y) * n.z);
}

float valueNoise(vec2 p)
{
    vec2 cell = floor(p);
    vec2 t = fract(p);
    t = t * t * (3.0 - 2.0 * t);
    float top = mix(grain(cell), grain(cell + vec2(1.0, 0.0)), t.x);
    float bottom = mix(grain(cell + vec2(0.0, 1.0)), grain(cell + 1.0), t.x);
    return mix(top, bottom, t.y);
}

float amberIdleField(vec2 uv)
{
    vec2 right = (uv - vec2(1.02, 0.69)) * vec2(2.9, 2.0);
    vec2 upper = (uv - vec2(0.51, 0.16)) * vec2(3.1, 4.6);
    vec2 corner = (uv - vec2(0.13, 0.81)) * vec2(4.4, 4.0);
    float mottling = IDLE_COARSE_VARIATION * (valueNoise(uv * vec2(4.6, 3.7) + vec2(7.6, 4.8)) - 0.5)
                   + IDLE_FINE_VARIATION * (valueNoise(uv * vec2(13.0, 11.0) + vec2(3.5, 9.8)) - 0.5);
    return IDLE_LIGHT + IDLE_SIDE_GLOW * exp(-dot(right, right))
                      - IDLE_UPPER_SHADE * exp(-dot(upper, upper))
                      + IDLE_CORNER_GLOW * exp(-dot(corner, corner)) + mottling;
}

float roundedBox(vec2 p, vec2 center, vec2 halfSize, float radius)
{
    vec2 q = abs(p - center) - halfSize + radius;
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

float inside(float distanceToEdge, float aa)
{
    return 1.0 - smoothstep(-aa, aa, distanceToEdge);
}

float emission(vec4 state)
{
    return clamp(state.r + state.g - state.b, 0.0, 1.0);
}

float cellEmission(ivec2 cell, ivec2 limit)
{
    ivec2 safeCell = clamp(cell, ivec2(0), limit - 1);
    float value = emission(texelFetch(uLight, safeCell, 0));
    vec2 p = vec2(safeCell);
    float variation = (grain(p) - 0.5) * DOT_VARIATION
                    + (grain(vec2(p.x, 9.0)) - 0.5) * COLUMN_VARIATION
                    + (grain(vec2(14.0, p.y)) - 0.5) * ROW_VARIATION;
    return value * (1.0 + variation);
}

vec2 overlap(vec2 delta, vec2 halfDot, vec2 footprint)
{
    vec2 extent = footprint * 0.5;
    return max(min(delta + extent, halfDot) - max(delta - extent, -halfDot), 0.0)
         / footprint;
}

vec3 panel(vec2 uv, float pixelsPerDot)
{
    ivec2 cells = textureSize(uLight, 0);
    vec2 pitch = 1.0 / vec2(cells);
    vec2 p = clamp(uv * vec2(cells), vec2(0.0), vec2(cells) - 0.0001);
    ivec2 cell = ivec2(p);
    vec2 delta = fract(p) - 0.5;
    ivec2 side = ivec2(mix(vec2(-1.0), vec2(1.0), greaterThanEqual(delta, vec2(0.0))));

    vec4 filteredState = texture(uLight, uv);
    float interpolated = emission(filteredState);
    float aperture = interpolated * 0.72;
    if (pixelsPerDot > 0.85) {
        // Integrate the rectangular aperture over an output pixel. Include
        // the adjacent dots when the footprint crosses an electrode gap.
        vec2 footprint = vec2(1.0 / pixelsPerDot);
        vec2 halfDot = vec2(DOT_HALF_WIDTH, DOT_HALF_HEIGHT);
        vec2 center = overlap(delta, halfDot, footprint);
        vec2 neighbor = overlap(delta - vec2(side), halfDot, footprint);
        float dotLight = center.x * center.y * cellEmission(cell, cells)
                       + neighbor.x * center.y * cellEmission(cell + ivec2(side.x, 0), cells)
                       + center.x * neighbor.y * cellEmission(cell + ivec2(0, side.y), cells)
                       + neighbor.x * neighbor.y * cellEmission(cell + side, cells);
        aperture = mix(aperture, dotLight, smoothstep(0.85, 1.4, pixelsPerDot));
    }

    // Light spread through the cover glass, in units of physical dot pitch.
    vec2 dx = vec2(pitch.x * GLASS_SCATTER_PITCH, 0.0);
    vec2 dy = vec2(0.0, pitch.y * GLASS_SCATTER_PITCH);
    float scatter = emission(texture(uLight, uv + dx))
                  + emission(texture(uLight, uv - dx))
                  + emission(texture(uLight, uv + dy))
                  + emission(texture(uLight, uv - dy));

    float scanned = uPhase - (float(cell.y) + 0.5) * pitch.y;
    float age = fract(scanned);
    float polarity = scanned < 0.0 ? -uPolarity : uPolarity;
    float pulse = 1.0 + PULSE_RIPPLE * exp(-age * 9.0) + POLARITY_RIPPLE * polarity;
    float field = 1.04 - 0.14 * dot(uv - 0.46, uv - 0.46);
    float light = aperture * DOT_APERTURE_GAIN + interpolated * DOT_INTERPOLATED_GAIN
                + scatter * GLASS_SCATTER_GAIN;
    float exposure = uBloom ? BLOOM_EXPOSURE : BASE_EXPOSURE;
    if (uBloom) {
        // The tighter spread fills gaps between neighboring lit dots while
        // the wider bloom gives the cluster a halo.
        float softened = SOFT_INTERPOLATED_GAIN * interpolated + SOFT_SCATTER_GAIN * scatter;
        light = mix(light, softened, uRolling ? ROLLING_FIELD_SOFTEN : DARK_FIELD_SOFTEN);
        vec2 scatterWeight = uRolling ? vec2(ROLLING_FIELD_BLOOM_NEAR, ROLLING_FIELD_BLOOM_WIDE)
                                      : vec2(DARK_FIELD_BLOOM_NEAR, DARK_FIELD_BLOOM_WIDE);
        light += dot(texture(uBloomMap, uv).rg, scatterWeight);
    }
    light *= pulse * field * uBrightness * uVoltage * (1.0 - LOAD_SAG * uLoad);
    if (uRolling) {
        // Sample the apparent lower exposure of the rolling band in panel
        // coordinates. The amber idle field is also modulated by the band.
        float height = uBand.y;
        float band = 1.0 - smoothstep(height * BAND_EDGE_IN, height * BAND_EDGE_OUT, abs(uv.y - uBand.x));
        // Recently extinguished dots temporarily darken the powered field.
        float retained = max(filteredState.a - filteredState.r, 0.0);
        float suppression = retained * (RESIDUAL_SOFT + RESIDUAL_DARK * retained);
        float idleField = amberIdleField(uv) * (1.0 - suppression);
        light = (light + idleField * uVoltage * uBrightness) * (1.0 - BAND_DEPTH * band);
    }

    float reflection = exp(-dot((uv - vec2(0.12, -0.24)) * vec2(1.25, 2.5),
                               (uv - vec2(0.12, -0.24)) * vec2(1.25, 2.5)));
    float dust = grain(floor(gl_FragCoord.xy)) - 0.5;
    float electrodes = (abs(delta.x) < 0.43 ? 1.0 : 0.0)
                     * smoothstep(1.0, 2.8, pixelsPerDot);
    vec3 glass = vec3(0.00165, 0.00103, 0.00062)
               + (0.00088 * reflection + 0.00014 * dust + 0.00016 * electrodes)
                 * vec3(1.0, 0.79, 0.62);
    vec3 amber = uBloom ? vec3(BLOOM_AMBER_RGB) : vec3(BASE_AMBER_RGB);
    vec3 phosphor = 1.0 - exp(-exposure * light * amber);
    return pow(glass + phosphor, vec3(1.0 / DISPLAY_GAMMA));
}

vec3 housing(vec2 p, float aa)
{
    float surfaceNoise = grain(floor(p * 1.8)) - 0.5;
    vec2 caseCenter = caseSize * 0.5;
    vec2 bodyHalf = caseCenter - vec2(CASE_MARGIN_X, CASE_MARGIN_Y);
    float body = roundedBox(p, caseCenter, bodyHalf, 15.0);
    vec3 col = vec3(0.027, 0.032, 0.038);
    float halo = exp(-0.50 * length((p - vec2(510.0, 210.0)) / vec2(475.0, 390.0)));
    col += halo * vec3(0.033, 0.035, 0.037);

    float shade = roundedBox(p, caseCenter + vec2(0.0, 7.0), bodyHalf + vec2(2.0, 0.0), 17.0);
    col *= 1.0 - 0.72 * exp(-0.095 * max(shade, 0.0));
    if (body > 2.0 * aa)
        return col;

    float elevation = 1.0 - p.y / caseSize.y;
    vec3 coat = vec3(0.091, 0.096, 0.100) + elevation * vec3(0.041, 0.043, 0.046)
              + 0.021 * surfaceNoise;
    float lip = smoothstep(-5.0, -0.8, body);
    float daylight = 1.0 - smoothstep(180.0, 810.0, p.y + p.x * 0.24);
    coat += lip * (daylight * 0.092 - 0.048);
    coat = mix(coat, vec3(0.052, 0.056, 0.058), smoothstep(-1.5, 0.4, body) * 0.68);

    vec2 glassHalf = 0.5 * glassSize;
    vec2 assembly = glassOrigin + glassHalf;
    vec2 bezelHalf = glassHalf + vec2(BEZEL_LIP_X, BEZEL_LIP_Y);
    float surround = roundedBox(p, assembly, bezelHalf, 10.0);
    float seam = roundedBox(p, assembly + vec2(0.0, 1.0), bezelHalf + 2.0, 11.0);
    coat *= 1.0 - inside(seam, aa) * 0.58;
    vec3 surroundColor = vec3(0.049, 0.047, 0.043) + 0.010 * surfaceNoise;
    float rim = smoothstep(-11.0, -0.4, surround);
    surroundColor += rim * (0.078 - 0.046 * elevation);
    coat = mix(coat, surroundColor, inside(surround, aa));

    float glassEdge = roundedBox(p, assembly, glassHalf + 5.0, 3.8);
    vec3 gasket = vec3(0.024, 0.019, 0.016) + 0.029 * smoothstep(-3.0, 0.0, glassEdge);
    coat = mix(coat, gasket, inside(glassEdge, aa));

    vec2 screwInset = vec2(CASE_MARGIN_X + 29, CASE_MARGIN_Y + 27);
    vec2 screwP = mix(screwInset, caseSize - screwInset, greaterThanEqual(p, caseCenter));
    float screw = length(p - screwP);
    coat = mix(coat, vec3(0.042, 0.044, 0.046), inside(screw - 6.4, aa));
    coat = mix(coat, vec3(0.147, 0.150, 0.150) + 0.013 * surfaceNoise,
               inside(screw - 4.5, aa));
    vec2 slot = p - screwP;
    slot = vec2(slot.x + slot.y * 0.54, slot.y - slot.x * 0.54);
    float cut = roundedBox(slot, vec2(0.0), vec2(4.0, 0.85), 0.2);
    coat = mix(coat, vec3(0.038, 0.041, 0.044), inside(cut, aa));

    float led = length(p - vec2(PILOT_X, PILOT_Y));
    coat = mix(coat, vec3(0.044, 0.041, 0.035), inside(led - 5.9, aa));
    vec3 pilot = mix(vec3(0.15, 0.078, 0.031), vec3(0.99, 0.57, 0.16), uVoltage);
    coat = mix(coat, pilot * (0.84 + 0.16 * exp(-0.25 * led * led)), inside(led - 3.0, aa));
    coat += vec3(0.064, 0.035, 0.009) * uVoltage * exp(-0.13 * led * led);

    float buttonY = float(BUTTON_Y) + 0.5 * float(BUTTON_H);
    vec2 keyHalf = 0.5 * vec2(BUTTON_W, BUTTON_H);
    int index = int(floor((p.x - float(BUTTON_X)) / float(BUTTON_STEP)));
    bool keyArea = index >= 0 && index < BUTTON_COUNT && abs(p.y - buttonY) < keyHalf.y + 4.0;
    float litKey = 0.0;
    if (keyArea) {
        vec2 keyCenter = vec2(float(BUTTON_X + index * BUTTON_STEP) + keyHalf.x, buttonY);
        float key = roundedBox(p, keyCenter, keyHalf, 3.0);
        bool engaged = (uEngaged & (1 << index)) != 0;
        litKey = float(engaged) * inside(key, aa);
        coat = mix(coat, vec3(0.044, 0.045, 0.045), inside(key - 1.0, aa));
        vec3 top = vec3(0.132, 0.136, 0.140);
        top += engaged ? vec3(0.104, 0.051, 0.006) : vec3(0.0);
        if (index == uHover)
            top += vec3(0.046, 0.042, 0.037);
        top += 0.018 * surfaceNoise - 0.0012 * (p.y - keyCenter.y);
        coat = mix(coat, top, inside(key + 1.1, aa));
        coat = mix(coat, top * 0.77, inside(key + 4.0, aa));
        float dividerX = float(BUTTON_LABEL_X) - 4.5 - keyHalf.x;
        float divider = roundedBox(p, keyCenter + vec2(dividerX, 0.0), vec2(0.48, 7.0), 0.2);
        coat = mix(coat, top * 0.48, inside(divider, aa));
    }

    vec2 inkUV = vec2((p.x - float(INK_X)) / float(INK_W), p.y / caseSize.y);
    float ink = texture(uSilkscreen, inkUV).r;
    vec3 paint = mix(vec3(0.69, 0.69, 0.66), vec3(0.91, 0.70, 0.40), litKey);
    coat = mix(coat, paint, ink);
    return mix(col, coat, inside(body, aa));
}

void main()
{
    vec2 topLeft = vec2(gl_FragCoord.x, uDrawable.y - gl_FragCoord.y);
    vec2 fit = uGlassOnly ? glassSize : caseSize;
    vec2 scaleXY = uDrawable / fit;
    float scale = min(scaleXY.x, scaleXY.y);
    vec2 p = (topLeft - 0.5 * (uDrawable - fit * scale)) / scale;
    if (uGlassOnly)
        p += glassOrigin;

    vec2 uv = (p - glassOrigin) / glassSize;
    bool onGlass = all(greaterThanEqual(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)));
    vec3 color;
    if (onGlass)
        color = panel(uv, scale * glassSize.x / float(textureSize(uLight, 0).x));
    else if (uGlassOnly)
        color = vec3(0.019, 0.015, 0.013);
    else
        color = housing(p, 0.7 / scale);
    fragColor = vec4(color, 1.0);
}
