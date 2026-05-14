#version 450 core

// Bloom composite (F2H55 Bloque A): mezcla final del bloom acumulado con
// el HDR original de la escena. Es el ultimo paso antes del
// PostProcessPass (tonemap + gamma).
//
// El bloom acumulado es el mip 0 del chain de bloom (resultado de
// downsamples + upsamples in-place). Se suma al HDR con un blend
// controlado por `uBloomIntensity`.
//
// Convencion: lerp(hdr, hdr + bloom, intensity). Con intensity=0 la
// escena se ve identica al HDR original (cero regresion). Con
// intensity=1 el bloom se suma con peso completo.

in vec2 vUv;

uniform sampler2D uHdrTex;        // HDR original de la escena
uniform sampler2D uBloomTex;      // mip 0 del bloom chain (post upsample)
uniform float     uBloomIntensity;

out vec4 FragColor;

void main() {
    vec3 hdr   = texture(uHdrTex,   vUv).rgb;
    vec3 bloom = texture(uBloomTex, vUv).rgb;

    vec3 result = mix(hdr, hdr + bloom, uBloomIntensity);
    FragColor = vec4(result, 1.0);
}
