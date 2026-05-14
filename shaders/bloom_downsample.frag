#version 450 core

// Bloom downsample (F2H55 Bloque A): toma el HDR (o un mip superior) y
// produce un mip 1/2 con un filtro 13-tap "partial Karis average". Esto
// es la presentacion de COD Advanced Warfare 2014 (Jorge Jimenez,
// Sledgehammer/Activision), estandar de la industria desde hace 10 anos.
//
// Algoritmo portado de Godot 4 (godotengine/godot, MIT,
// servers/rendering/renderer_rd/shaders/effects/glow.glsl) + Filament
// (google/filament, Apache 2.0, filament/src/materials/dofCombine.mat).
//
// Layout de los 13 samples sobre la textura origen:
//
//   A . B . C
//   . D . E .
//   F . G . H
//   . I . J .
//   K . L . M
//
// Donde A..M son taps en pixels del mip origen (offset = texel_size).
// Se forman 5 "boxes" promediados:
//   box5 (centro): (D+E+I+J)/4
//   box1..4 (esquinas): (A+B+F+G)/4, (B+C+G+H)/4, (F+G+K+L)/4, (G+H+L+M)/4
//
// Resultado: box5 * 0.5 + (box1+box2+box3+box4) * 0.125
//
// Karis average (solo en el primer downsample, controlado por
// `uIsFirstPass`): cada box se pondera por 1/(1+luma) ANTES de mezclar.
// Esto aplaca "fireflies" (pixels HDR muy brillantes aislados que se
// vuelven super-brillantes despues del blur).
//
// Threshold soft-knee (tambien solo en primer pass): cualquier pixel
// debajo de `uThreshold` se atenua. Curva suave (knee) para evitar
// parpadeo cuando un pixel cruza el umbral entre frames.

in vec2 vUv;

uniform sampler2D uSrcTex;
uniform vec2      uSrcTexelSize;  // 1.0 / textureSize(uSrcTex)
uniform int       uIsFirstPass;   // 1 = aplicar Karis + threshold; 0 = downsample plano
uniform float     uThreshold;     // luminance threshold (HDR units)
uniform float     uSoftKnee;      // typical 0.5

out vec4 FragColor;

float luma(vec3 c) {
    // Rec.709 luminance. Mismo coeficientes que Filament/Godot.
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 karisWeight(vec3 c) {
    // Karis: w = 1 / (1 + luma). Atenua bright outliers antes del average.
    return c / (1.0 + luma(c));
}

vec3 thresholdSoftKnee(vec3 c) {
    // Soft-knee curve (Unreal/Unity standard). Por debajo del threshold
    // se atenua suavemente; por encima pasa lineal.
    float brightness = max(c.r, max(c.g, c.b));
    float knee = uThreshold * uSoftKnee;
    float soft = brightness - uThreshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    float contribution = max(soft, brightness - uThreshold);
    contribution /= max(brightness, 0.00001);
    return c * contribution;
}

void main() {
    vec2 t = uSrcTexelSize;

    // 13 taps (ver layout en el comment del header).
    vec3 A = texture(uSrcTex, vUv + t * vec2(-2.0, -2.0)).rgb;
    vec3 B = texture(uSrcTex, vUv + t * vec2( 0.0, -2.0)).rgb;
    vec3 C = texture(uSrcTex, vUv + t * vec2( 2.0, -2.0)).rgb;
    vec3 D = texture(uSrcTex, vUv + t * vec2(-1.0, -1.0)).rgb;
    vec3 E = texture(uSrcTex, vUv + t * vec2( 1.0, -1.0)).rgb;
    vec3 F = texture(uSrcTex, vUv + t * vec2(-2.0,  0.0)).rgb;
    vec3 G = texture(uSrcTex, vUv                       ).rgb;
    vec3 H = texture(uSrcTex, vUv + t * vec2( 2.0,  0.0)).rgb;
    vec3 I = texture(uSrcTex, vUv + t * vec2(-1.0,  1.0)).rgb;
    vec3 J = texture(uSrcTex, vUv + t * vec2( 1.0,  1.0)).rgb;
    vec3 K = texture(uSrcTex, vUv + t * vec2(-2.0,  2.0)).rgb;
    vec3 L = texture(uSrcTex, vUv + t * vec2( 0.0,  2.0)).rgb;
    vec3 M = texture(uSrcTex, vUv + t * vec2( 2.0,  2.0)).rgb;

    if (uIsFirstPass == 1) {
        // Threshold soft-knee + Karis weighted average.
        A = karisWeight(thresholdSoftKnee(A));
        B = karisWeight(thresholdSoftKnee(B));
        C = karisWeight(thresholdSoftKnee(C));
        D = karisWeight(thresholdSoftKnee(D));
        E = karisWeight(thresholdSoftKnee(E));
        F = karisWeight(thresholdSoftKnee(F));
        G = karisWeight(thresholdSoftKnee(G));
        H = karisWeight(thresholdSoftKnee(H));
        I = karisWeight(thresholdSoftKnee(I));
        J = karisWeight(thresholdSoftKnee(J));
        K = karisWeight(thresholdSoftKnee(K));
        L = karisWeight(thresholdSoftKnee(L));
        M = karisWeight(thresholdSoftKnee(M));
    }

    // 5 boxes de 2x2 promediados.
    vec3 box5 = (D + E + I + J) * 0.25;            // centro (inner)
    vec3 box1 = (A + B + F + G) * 0.25;            // top-left
    vec3 box2 = (B + C + G + H) * 0.25;            // top-right
    vec3 box3 = (F + G + K + L) * 0.25;            // bottom-left
    vec3 box4 = (G + H + L + M) * 0.25;            // bottom-right

    vec3 result = box5 * 0.5 + (box1 + box2 + box3 + box4) * 0.125;
    FragColor = vec4(result, 1.0);
}
