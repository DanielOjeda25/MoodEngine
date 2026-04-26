#version 450 core

// Fullscreen triangle (Hito 15 Bloque 3): NO usa VBO. El vertex shader
// genera la posicion + UV directamente desde `gl_VertexID` (0, 1, 2). Es
// un truco estandar para evitar mantener un VAO/VBO trivial.
//
// El triangulo cubre el viewport completo: vertices en NDC (-1,-1),
// (3,-1), (-1,3). El fragment shader recorta lo que cae fuera.

out vec2 vUv;

void main() {
    // ID 0 -> (-1,-1) UV (0,0)
    // ID 1 -> ( 3,-1) UV (2,0)
    // ID 2 -> (-1, 3) UV (0,2)
    vec2 pos = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                    (gl_VertexID == 2) ? 3.0 : -1.0);
    vUv = (pos + vec2(1.0)) * 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
