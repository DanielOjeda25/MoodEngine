#version 450 core

// Hito 3: posicion transformada por matrices MVP, color por vertice y
// coordenadas UV pasadas al fragment para sampleo de textura.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;
out vec2 vUv;

void main() {
    vColor = aColor;
    vUv = aUv;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
