#version 450 core

// Fragment shader del skybox modo equirectangular. Recibe la direccion
// del vertex shader (igual al `skybox.frag` cubemap), pero en vez de
// `samplerCube` usa `sampler2D` con un mapping spherical:
//   lon = atan2(z, x)  in [-pi, pi]
//   lat = asin(y)      in [-pi/2, pi/2]
//   u = lon / (2*pi) + 0.5
//   v = 0.5 + lat / pi
// Asi una textura panoramica 2:1 (ej. 2048x1024) se proyecta sin
// seams sobre la esfera del cielo.
//
// Nota: usamos `+ lat/PI` (no `- lat/PI`) porque `OpenGLTexture` carga
// los PNGs con `stbi_set_flip_vertically_on_load(true)`, asi que la
// fila 0 del PNG (zenith) queda en v=1, y la ultima fila (nadir) en v=0.

in vec3 vDir;

uniform sampler2D uSkybox;

out vec4 fragColor;

const float PI = 3.14159265358979323846;

void main() {
    vec3 d = normalize(vDir);
    float lon = atan(d.z, d.x);
    float lat = asin(clamp(d.y, -1.0, 1.0));
    float u = lon / (2.0 * PI) + 0.5;
    float v = 0.5 + lat / PI;
    fragColor = texture(uSkybox, vec2(u, v));
}
