#version 450 core

// F2H75: vertex shader de telas (cloth). Los vertices ya vienen en WORLD
// space (el ClothSystem los produce desde la pose del soft body de Jolt),
// asi que no hay uModel: solo view * projection. Normal recalculada por
// frame en CPU y pasada al fragment para el lit.

layout(location = 0) in vec3 aPos;     // world space
layout(location = 1) in vec3 aNormal;  // world space

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;

void main() {
    vNormal = aNormal;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
