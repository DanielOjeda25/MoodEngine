#version 450 core

// Color Grading (F2H58 Bloque B): mapea cada color HDR a traves de una
// LUT 2D 256x16 (16 slices de 16x16, layout horizontal). Convencion
// Unity URP / Unreal pre-tonemap.
//
// La LUT es una textura RGBA8 de 256x16 que codifica un cubo RGB 16x16x16:
//   - Eje horizontal: R varia en cada slice (0..15) + indice de slice
//     (blue) repetido 16 veces -> total 256 px.
//   - Eje vertical: G varia (0..15).
//   - Slice: B determina cual de los 16 slices se sample.
//
// Algoritmo:
//   1. clamp(color, 0, 1) -- color grading no opera sobre HDR > 1, eso
//      es trabajo del tonemap. Aplicar grade a HDR luego seria mapear
//      colores fuera del cubo 16^3 -> resultados indefinidos.
//   2. Calcular blueSlice = B * 15. floor da el slice base, frac la
//      interpolacion al siguiente.
//   3. Sample del slice low y high, mix por frac.
//   4. mix(color, graded, intensity) -- blend con el original. 0 = sin
//      grade, 1 = grade puro.
//
// La LUT identidad cumple: lookup(c) == c. Util para reset por codigo
// + test que valida que el algoritmo lookup es correcto.

in vec2 vUv;

uniform sampler2D uHdrTex;       // HDR scene (post bloom + SSAO)
uniform sampler2D uLutTex;       // LUT 256x16 RGBA8
uniform float     uIntensity;    // [0,1]

out vec4 FragColor;

vec3 sampleLut(vec3 color) {
    // Clamp al cubo [0,1]. HDR > 1 se trunca -- color grading
    // no esta definido fuera del cubo.
    vec3 c = clamp(color, 0.0, 1.0);

    // Posicion en el eje Blue del cubo 16x16x16.
    float blueSlice = c.b * 15.0;
    float blueFloor = floor(blueSlice);
    float blueFrac  = blueSlice - blueFloor;

    // UV del slice low + high. R varia dentro del slice (1/16 del width
    // total), G varia en height (todo el rango).
    //   uLow.x  = R / 16 + sliceFloor / 16
    //   uHigh.x = R / 16 + (sliceFloor + 1) / 16
    // El +0.5/256 (low edge offset) lo absorbemos en el rango del
    // sampler con CLAMP_TO_EDGE -- mas simple que ajustarlo aqui.
    vec2 uvLow  = vec2(c.r / 16.0 + blueFloor       / 16.0, c.g);
    vec2 uvHigh = vec2(c.r / 16.0 + (blueFloor + 1.0) / 16.0, c.g);

    vec3 lowColor  = texture(uLutTex, uvLow).rgb;
    vec3 highColor = texture(uLutTex, uvHigh).rgb;

    return mix(lowColor, highColor, blueFrac);
}

void main() {
    vec3 hdr    = texture(uHdrTex, vUv).rgb;
    vec3 graded = sampleLut(hdr);
    vec3 result = mix(hdr, graded, uIntensity);
    FragColor = vec4(result, 1.0);
}
