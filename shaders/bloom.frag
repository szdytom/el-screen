#version 300 es
precision highp float;

uniform sampler2D uImage;
uniform vec2 uSize;
uniform bool uHorizontal;
uniform vec2 uWeight[BLOOM_RADIUS + 1];
out vec4 filtered;

vec2 lightAt(vec2 uv)
{
    vec3 state = texture(uImage, uv).rgb;
    if (!uHorizontal)
        return state.rg;
    float light = clamp(state.r + state.g - state.b, 0.0, 1.0);
    return vec2(light, light * (0.2 + 0.8 * light));
}

void main()
{
    vec2 uv = gl_FragCoord.xy / uSize;
    vec2 axis = (uHorizontal ? vec2(1.0, 0.0) : vec2(0.0, 1.0)) / uSize;
    vec2 glow = uWeight[0] * lightAt(uv);
    for (int i = 1; i <= BLOOM_RADIUS; ++i) {
        vec2 offset = axis * float(i);
        glow += uWeight[i] * (lightAt(uv - offset) + lightAt(uv + offset));
    }
    filtered = vec4(glow, 0.0, 1.0);
}
