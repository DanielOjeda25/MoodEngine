#version 450 core

// Vertex shader del skybox (Hito 15 Bloque 1).
//
// Estrategia "infinity sky": multiplica la posicion por una mat4 que tiene
// la traslacion de la view matrix anulada (vec4(viewPosNoTranslate, 1)).
// Asi el cubo del skybox se mueve con la rotacion de la camara pero no con
// su posicion — el fondo se ve infinitamente lejos.
//
// Truco del depth: forzamos `gl_Position.z = gl_Position.w` para que tras la
// division perspectiva el depth quede en 1.0 (far plane). Renderizado con
// depth test LEQUAL, cualquier geometria de la escena (depth < 1) escribe
// encima del skybox; lo que NO se dibuja queda con el sky de fondo.

layout(location = 0) in vec3 aPos;

uniform mat4 uViewNoTranslation;  // mat4(mat3(view)), sin componente de translacion
uniform mat4 uProjection;

out vec3 vDir;  // direccion del sample en el cubemap (igual a la posicion del vertice)

void main() {
    vDir = aPos;
    vec4 pos = uProjection * uViewNoTranslation * vec4(aPos, 1.0);
    // Forzar z = w para que tras /w quede en el far plane (depth 1.0).
    gl_Position = pos.xyww;
}
