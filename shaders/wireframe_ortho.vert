#version 450 core

// F2H28: vertex shader minimo para el render orto wireframe del
// workspace "Editor de mapas". Solo necesita posicion + matrices —
// sin normal/uv/color — porque el frag pinta un color flat. El
// layout 11-floats interleaved del brush mesh expone aColor/aUv/
// aNormal en locations 1/2/3 que NO declaramos (GL los ignora si
// el shader no los lee).

layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
