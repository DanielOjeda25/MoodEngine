#version 450 core

// Combina color por vertice con la textura sampleada. El color del vertex
// actua como tinte; para un look neutro, usar blanco en los vertices.

in vec3 vColor;
in vec2 vUv;

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 tex = texture(uTexture, vUv);
    FragColor = vec4(tex.rgb * vColor, tex.a);
}
