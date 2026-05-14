#version 450 core

// Blur del AO factor -- F2H56 Bloque A. SSAO produce ruido inherente al
// sampling (16 samples por pixel con rotacion por IGN noise). Un blur
// 4x4 caja simple alcanza para suavizar -- mas elaborado (bilateral
// con depth-aware weights) es mejor calidad pero overkill para v1.
//
// Lee el AO factor (R8 single-channel) y escribe el promedio 4x4 al
// mismo canal.

in vec2 vUv;

uniform sampler2D uAoTex;
uniform vec2      uTexelSize;  // 1.0 / textureSize(uAoTex)

out float FragColor;

void main() {
    float sum = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * uTexelSize;
            sum += texture(uAoTex, vUv + offset).r;
        }
    }
    FragColor = sum / 16.0;
}
