#version 450 core

// Hito 20: fragment del UI shader. Multiplica el color del vertex
// (premultiplicado por alpha) por el sample de la textura. Para
// geometria sin textura, `uHasTex=0` y el sample devuelve blanco.

in vec4 vColor;
in vec2 vUv;

uniform sampler2D uTex;
uniform int       uHasTex;

out vec4 FragColor;

void main() {
    vec4 t = (uHasTex == 1) ? texture(uTex, vUv) : vec4(1.0);
    FragColor = vColor * t;
}
