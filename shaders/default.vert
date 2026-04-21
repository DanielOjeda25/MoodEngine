#version 450 core

// Shader basico de Hito 2: pasa posicion directamente en NDC (sin matrices)
// y transmite el color del vertice al fragment shader para interpolacion.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = vec4(aPos, 1.0);
}
