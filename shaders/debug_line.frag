#version 450 core

// Color plano tomado del vertex; sin iluminacion.

in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
