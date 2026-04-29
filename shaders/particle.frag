#version 450 core

// Fragment shader de partículas: muestrea la textura del emisor en uv,
// la modula por el color del per-instance (que ya tiene la
// interpolacion start/end aplicada en CPU), y descarta los pixeles muy
// transparentes. El blending (alpha vs aditivo) lo decide el caller via
// `glBlendFunc` antes del draw.

in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main() {
    vec4 tex = texture(uTexture, vUV);
    vec4 c = tex * vColor;
    if (c.a < 0.01) discard;
    fragColor = c;
}
