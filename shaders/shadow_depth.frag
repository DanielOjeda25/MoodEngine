#version 450 core

// Fragment shader del pase de profundidad. Vacio a proposito: el depth
// se escribe automatico al pasar por las transformaciones del vertex
// shader. No hay color buffer (`glDrawBuffer(GL_NONE)` en el FBO depth
// del shadow map).

void main() {
    // Sin output. La GPU igual escribe `gl_FragDepth` automaticamente.
}
