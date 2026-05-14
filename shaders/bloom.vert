#version 450 core

// Vertex shader compartido por los 3 fragment shaders de bloom
// (downsample, upsample, composite) -- F2H55 Bloque A. Mismo patron que
// post_process.vert: fullscreen triangle generado desde `gl_VertexID`
// sin VAO/VBO. El triangulo cubre el viewport (-1,-1)..(3,3) en NDC.

out vec2 vUv;

void main() {
    vec2 pos = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                    (gl_VertexID == 2) ? 3.0 : -1.0);
    vUv = (pos + vec2(1.0)) * 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
