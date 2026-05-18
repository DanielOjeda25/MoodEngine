#version 450 core

// F2H64: pase de sombra TINTADA. Corre DESPUES del pase opaco F2H60 que
// llena la depth array. Aquí:
//   - depth-write OFF (GL state) -- los translucents NO ocluyen luz.
//   - depth-test ON con LEQUAL -- translucents detras de un opaco no
//     contribuyen al tinte (la luz no llega a ese pixel).
//   - blend ZERO/SRC_COLOR (multiplicativo) -- el color attachment se
//     limpia a (1,1,1,1) y cada translucent multiplica por su tinte.
//     Si dos vidrios verdes se solapan, el tinte se acumula como
//     (1*g1)*g2 -- correcto fisicamente (cada vidrio absorbe).
//
// Tinte por material: (albedoTint * (1 - opacity)). Un vidrio con
// opacity=0.25 y albedo cyan (0.75,0.88,1.0) tinta a (0.56, 0.66, 0.75)
// la luz que pasa. Opacity=1 (opaco) -> tinte=(0,0,0) -> sombra negra
// (equivalente al opaque shadow). Opacity=0 (totalmente transparente)
// -> tinte=albedo -> el vidrio "filtra" su color completo.

uniform vec3  uAlbedoTint;   // del MaterialAsset
uniform float uOpacity;       // 0=transparente, 1=opaco

layout(location = 0) out vec4 FragColor;

void main() {
    // tinte = albedo * (1 - opacity). Clamp a [0,1] por seguridad
    // (uOpacity puede venir saturado por Fresnel mas adelante en el
    // pipeline, no en el shadow pass v1).
    vec3 tint = clamp(uAlbedoTint * (1.0 - uOpacity), 0.0, 1.0);
    FragColor = vec4(tint, 1.0);
}
