#version 450 core

// Color plano tomado del vertex; sin iluminacion.

in vec4 vColor;  // F2H17: incluye alpha
out vec4 FragColor;

void main() {
    FragColor = vColor;
}
