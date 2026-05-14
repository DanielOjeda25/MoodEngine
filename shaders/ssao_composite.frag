#version 450 core

// SSAO composite -- F2H56 Bloque A. Multiplica el HDR scene color por
// el AO factor (escala de gris suavizado por blur). El AO factor [0..1]
// modula cuanto contribuye el ambient/iluminacion suave a cada pixel:
//   - 1.0 = pixel sin oclusion (color HDR pasa intacto)
//   - 0.0 = pixel totalmente ocluido (color oscurecido)
//
// Limitacion conocida v1: multiplicamos el color final, no solo el
// componente ambient. Esto significa que la luz directa tambien recibe
// AO (incorrecto fisicamente). Visualmente similar en mayoria de
// escenas; si emerge calidad pobre se refactorea a integrarse con el
// PBR shader (modular solo el termino ambient).

in vec2 vUv;

uniform sampler2D uHdrTex;
uniform sampler2D uAoTex;

out vec4 FragColor;

void main() {
    vec3  hdr = texture(uHdrTex, vUv).rgb;
    float ao  = texture(uAoTex,  vUv).r;
    FragColor = vec4(hdr * ao, 1.0);
}
