#version 450 core

// Vertex shader del particle system (Hito 29 Bloque 2).
//
// Estrategia: instanced rendering sin VBO de quad. Cada draw call dibuja
// `n*6` vertices (`glDrawArraysInstanced(GL_TRIANGLES, 0, 6, n)`):
// gl_VertexID 0..5 forma 2 triangulos del quad billboard. Per-instance
// (divisor=1) leemos el centro/size/color de la particula desde el VBO
// dinamico que `OpenGLParticleRenderer` rellena por frame.
//
// Billboard: tomamos los basis right/up del view matrix (filas 0/1
// transpuestas) y armamos el quad en world-space alineado a la camara.

layout(location = 0) in vec3 aCenter;  // per-instance, divisor 1
layout(location = 1) in float aSize;
layout(location = 2) in vec4 aColor;

uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

const vec2 k_offsets[6] = vec2[6](
    vec2(-0.5, -0.5), vec2( 0.5, -0.5), vec2( 0.5,  0.5),
    vec2(-0.5, -0.5), vec2( 0.5,  0.5), vec2(-0.5,  0.5)
);
const vec2 k_uvs[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
);

void main() {
    int idx = gl_VertexID % 6;
    vec2 off = k_offsets[idx];
    vUV    = k_uvs[idx];
    vColor = aColor;

    // Right/up de la camara: filas 0/1 del view matrix transpuestas.
    vec3 right = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 up    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    vec3 worldPos = aCenter + (right * off.x + up * off.y) * aSize;
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
