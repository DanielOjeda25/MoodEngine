#version 450 core

// Emite el color interpolado recibido del vertex shader. Sin iluminacion ni
// texturas todavia: eso entra en hitos posteriores.

in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
