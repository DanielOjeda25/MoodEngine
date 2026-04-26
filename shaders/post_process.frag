#version 450 core

// Post-process pass (Hito 15 Bloque 3): toma una textura HDR (RGBA16F)
// y produce LDR (RGBA8) tras:
//   1. Exposicion: scale lineal por 2^uExposure (en EVs).
//   2. Tonemap (`uTonemap`):
//        0 = None    (clamp directo).
//        1 = Reinhard simple: x / (1 + x).
//        2 = ACES filmica (Narkowicz, fast approx).
//   3. Gamma 1/2.2 a sRGB-aprox (no usamos un sRGB framebuffer real;
//      mas simple aplicar la curva en el shader que mantener un FB
//      sRGB y depender del ICC del SO).

in vec2 vUv;

uniform sampler2D uHdrTex;
uniform float uExposure; // EVs, [-5..+5] tipico
uniform int   uTonemap;  // 0..2

vec3 tonemapReinhard(vec3 c) {
    return c / (1.0 + c);
}

// ACES filmica simplificada (Narkowicz). Trabaja en RGB lineal HDR.
vec3 tonemapACES(vec3 c) {
    const float a = 2.51;
    const float b = 0.03;
    const float cc = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((c * (a * c + b)) / (c * (cc * c + d) + e), 0.0, 1.0);
}

out vec4 FragColor;

void main() {
    vec3 hdr = texture(uHdrTex, vUv).rgb;
    hdr *= exp2(uExposure);

    vec3 ldr;
    if (uTonemap == 1) {
        ldr = tonemapReinhard(hdr);
    } else if (uTonemap == 2) {
        ldr = tonemapACES(hdr);
    } else {
        ldr = clamp(hdr, 0.0, 1.0);
    }

    // Gamma correction. Aproximacion estandar 1/2.2; no es la curva sRGB
    // exacta pero la diferencia visual es minima y no necesitamos un FB
    // sRGB.
    ldr = pow(ldr, vec3(1.0 / 2.2));
    FragColor = vec4(ldr, 1.0);
}
