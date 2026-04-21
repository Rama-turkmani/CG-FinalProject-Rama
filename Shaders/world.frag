#version 330 core

in vec2  vUV;
in vec3  vNormal;
in vec3  vWorldPos;
in float vViewDist;

out vec4 FragColor;

uniform sampler2D uTex;
uniform int   uUseTexture;   // 1 = sample texture, 0 = flat color
uniform vec3  uTint;         // base / tint color
uniform vec3  uLightDir;     // direction the light travels (world space)
uniform vec3  uFogColor;
uniform float uFogDensity;   // exponential fog density
uniform int   uDiscardAlpha; // 1 = alpha-test (for coin billboards)

void main()
{
    vec4 sampled = uUseTexture == 1 ? texture(uTex, vUV) : vec4(1.0);
    vec3 base = sampled.rgb * uTint;

    // Simple directional diffuse + ambient lighting (Pair 4: visual effect).
    vec3  N      = normalize(vNormal);
    vec3  L      = normalize(-uLightDir);
    float diff   = max(dot(N, L), 0.0);
    vec3  color  = base * (0.35 + 0.75 * diff);

    // Exponential distance fog (Pair 4: second visual effect).
    float f = 1.0 - exp(-uFogDensity * vViewDist);
    f = clamp(f, 0.0, 1.0);
    color = mix(color, uFogColor, f);

    // Alpha test so coin billboards keep transparent background.
    if (uDiscardAlpha == 1 && sampled.a < 0.15) discard;

    FragColor = vec4(color, 1.0);
}
