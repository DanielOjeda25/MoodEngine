#version 450 core

// Hito 17: vertex shader del pipeline PBR. Igual estructura que `lit.vert`
// pero deja la puerta abierta para tangents (normal mapping en Bloque
// futuro). Por ahora solo pasa pos + uv + normal en world-space.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

// F2H60: con CSM, el shader frag elige cascada por depth view-space y
// reconstruye la pos en light-space desde vWorldPos con la matriz
// apropiada del array `uLightSpaces`. uLightSpace single y vLightSpace
// Pos out se eliminan.

out vec3  vColor;
out vec2  vUv;
out vec3  vWorldPos;
out vec3  vWorldNormal;
out float vViewSpaceZ;  // F2H60: |view-space Z| para seleccion de cascada CSM.

void main() {
    vColor = aColor;
    vUv = aUv;

    vec4 worldPos4 = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;

    // Normal en mundo: inverse-transpose del 3x3 superior del model.
    // Soporta scales no uniformes correctamente.
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    vWorldNormal = normalize(normalMatrix * aNormal);

    vec4 viewPos = uView * worldPos4;
    vViewSpaceZ = abs(viewPos.z);

    gl_Position = uProjection * viewPos;
}
