#version 450 core

// F2H4: variante instanced del pbr.vert. La unica diferencia con
// pbr.vert es que `mat4 model` viaja como atributo de instancia
// (locations 5-8, divisor=1) en lugar de uniform. El frag (pbr.frag)
// es identico — recibe los mismos varyings y uniforms.
//
// Si pbr.vert cambia (nuevo varying, nuevo input), este archivo debe
// actualizarse en paralelo. Single source of truth: si emerge churn,
// extraer la matematica compartida a un .glsl include.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 aNormal;

// Per-instance: matriz model. Las 4 columnas viajan como atributos
// individuales en locations [5..8] (GL no permite un solo
// `mat4 attribute` directamente). Divisor=1 setteado en
// OpenGLInstanceBuffer::bindAsAttributeMat4.
layout(location = 5) in mat4 aModel;

uniform mat4 uView;
uniform mat4 uProjection;

// F2H60: idem pbr.vert. uLightSpace eliminado; vViewSpaceZ pasa al frag
// para que elija cascada CSM.

out vec3  vColor;
out vec2  vUv;
out vec3  vWorldPos;
out vec3  vWorldNormal;
out float vViewSpaceZ;

void main() {
    vColor = aColor;
    vUv = aUv;

    vec4 worldPos4 = aModel * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;

    // Normal matrix: inverse-transpose del 3x3 superior. Calculada por
    // instancia — caro pero correcto para scales no uniformes. Si
    // emerge cuello, restringir a "uniform scale only" y usar mat3(aModel).
    mat3 normalMatrix = mat3(transpose(inverse(aModel)));
    vWorldNormal = normalize(normalMatrix * aNormal);

    vec4 viewPos = uView * worldPos4;
    vViewSpaceZ = abs(viewPos.z);

    gl_Position = uProjection * viewPos;
}
