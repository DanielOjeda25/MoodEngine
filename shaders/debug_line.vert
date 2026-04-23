#version 450 core

// Debug renderer (Hito 4 Bloque 5): lineas y cajas para visualizar fisica
// y otros datos. pos/color interleaved por vertice; matriz MVP simple.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
