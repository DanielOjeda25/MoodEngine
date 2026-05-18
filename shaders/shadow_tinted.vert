#version 450 core

// F2H64: pase de sombra TINTADA para materiales translucent.
// Vert idéntico al `shadow_depth.vert` -- solo necesitamos posición.
// El frag escribe el color tinte al RGBA8 attachment del shadow FBO.

layout(location = 0) in vec3 aPos;

uniform mat4 uLightSpace; // = lightProj * lightView
uniform mat4 uModel;

void main() {
    gl_Position = uLightSpace * uModel * vec4(aPos, 1.0);
}
