#version 450 core

// Hito 11: shader iluminado. Pasa al fragment la posicion en world (para
// distancia a luces puntuales y vector a la camara) y la normal transformada
// por la inversa-transpuesta del modelo (para escalas no uniformes).

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;
out vec2 vUv;
out vec3 vWorldPos;
out vec3 vWorldNormal;

void main() {
    vColor = aColor;
    vUv = aUv;

    vec4 worldPos4 = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;

    // Normal en mundo: inverse-transpose del 3x3 superior del model.
    // Coste: una mat3->inverse->transpose por vertice. Aceptable para
    // los volumenes del Hito 11 (decenas de entidades). Si crece, mover
    // a un uniform `uNormalMatrix` que el CPU pre-calcula.
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    vWorldNormal = normalize(normalMatrix * aNormal);

    gl_Position = uProjection * uView * worldPos4;
}
