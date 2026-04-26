#version 450 core

// Fragment shader del skybox: samplea un cubemap usando la direccion del
// vertice como vector de muestreo. La direccion ya viene en world-space
// (porque al skybox le quitamos la translacion de la camara, no la
// rotacion).

in vec3 vDir;

uniform samplerCube uSkybox;

out vec4 fragColor;

void main() {
    fragColor = texture(uSkybox, vDir);
}
