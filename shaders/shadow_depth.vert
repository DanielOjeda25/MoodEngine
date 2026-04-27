#version 450 core

// Pase de profundidad para shadow mapping (Hito 16 Bloque 1).
//
// Se renderiza la escena desde la perspectiva de la luz direccional. Solo
// importa la POSICION del vertice — no muestreamos texturas, no calculamos
// iluminacion, no nos interesa color. El depth se escribe automaticamente
// al pasar por la transformacion `lightProj * lightView * model`.
//
// El layout coincide con el del cubo primitivo y los meshes importados:
// pos(3) en location 0. Las demas locations (color/uv/normal) se ignoran
// — el VAO bound desde OpenGLMesh las tiene activadas, pero este shader
// solo declara location 0 y eso es legal.

layout(location = 0) in vec3 aPos;

uniform mat4 uLightSpace; // = lightProj * lightView
uniform mat4 uModel;

void main() {
    gl_Position = uLightSpace * uModel * vec4(aPos, 1.0);
}
