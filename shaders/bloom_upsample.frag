#version 450 core

// Bloom upsample (F2H55 Bloque A): toma un mip mas chico y lo upsamplea
// con un filtro tent 9-tap. El caller usa blending aditivo
// (glBlendFunc(GL_ONE, GL_ONE)) para sumar la contribucion sobre el mip
// destino que ya contiene su propio downsample.
//
// Algoritmo portado de Godot 4 (godotengine/godot, MIT) + COD Advanced
// Warfare 2014 presentation.
//
// Tent filter 9-tap:
//
//   1 2 1
//   2 4 2  / 16
//   1 2 1
//
// El `uRadius` escala el offset de los taps: 1.0 = ajustado, 2.0 = halo
// amplio (mas blurry).

in vec2 vUv;

uniform sampler2D uSrcTex;        // mip mas chico (origen del upsample)
uniform vec2      uSrcTexelSize;  // 1.0 / textureSize(uSrcTex)
uniform float     uRadius;        // 0.5..3.0 typical

out vec4 FragColor;

void main() {
    vec2 t = uSrcTexelSize * uRadius;

    // 9 taps en grid 3x3 alrededor del fragmento.
    vec3 a = texture(uSrcTex, vUv + vec2(-t.x, -t.y)).rgb;
    vec3 b = texture(uSrcTex, vUv + vec2( 0.0, -t.y)).rgb;
    vec3 c = texture(uSrcTex, vUv + vec2( t.x, -t.y)).rgb;
    vec3 d = texture(uSrcTex, vUv + vec2(-t.x,  0.0)).rgb;
    vec3 e = texture(uSrcTex, vUv                  ).rgb;
    vec3 f = texture(uSrcTex, vUv + vec2( t.x,  0.0)).rgb;
    vec3 g = texture(uSrcTex, vUv + vec2(-t.x,  t.y)).rgb;
    vec3 h = texture(uSrcTex, vUv + vec2( 0.0,  t.y)).rgb;
    vec3 i = texture(uSrcTex, vUv + vec2( t.x,  t.y)).rgb;

    // Tent: corners 1, edges 2, center 4 -- total 16.
    vec3 result = e * 4.0
                + (b + d + f + h) * 2.0
                + (a + c + g + i) * 1.0;
    result *= (1.0 / 16.0);

    FragColor = vec4(result, 1.0);
}
