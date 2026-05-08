#version 450 core

// F2H28: fragment shader del render orto wireframe. Color uniforme
// flat (sin lighting / textura / fog) — celeste GMod para brushes y
// meshes regulares, naranja Valve para entidades del SelectionSet.
// El caller bindea el color via uColor antes de cada draw call.

uniform vec3 uColor;

out vec4 FragColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
